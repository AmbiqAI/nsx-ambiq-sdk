"""Compiler witnesses for the generic scrubber's limited C transformations."""
from __future__ import annotations

import importlib.util
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

BEGIN = '// #### INTERNAL BEGIN ####\n'
END = '// #### INTERNAL END ####\n'
FENCED_NULL = BEGIN + ';\n' + END
COMPILERS = [name for name in ('gcc', 'clang') if shutil.which(name)] or [None]


@pytest.fixture()
def markers(repo_root: Path):
    spec = importlib.util.spec_from_file_location('semantics_markers', repo_root / 'sdk-intake/internal_markers.py')
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


@pytest.fixture(params=COMPILERS)
def compiler(request):
    if request.param is None:
        pytest.skip('C compiler unavailable')
    return request.param


@pytest.mark.parametrize('statement,expected', [
    ('if (x)\n' + FENCED_NULL + 'hits++;', (1, 1)),
    ('if (x) hits += 2; else\n' + FENCED_NULL + 'hits++;', (1, 3)),
    ('while (x-- > 0)\n' + FENCED_NULL + 'hits++;', (1, 1)),
    ('for (int i = 0; i < x; i++)\n' + FENCED_NULL + 'hits++;', (1, 1)),
    ('do\n' + FENCED_NULL + 'while (x-- > 0); hits++;', (1, 1)),
    ('if (x) {\n' + FENCED_NULL + '} hits++;', (1, 1)),
])
def test_null_statement_runtime_is_preserved(markers, tmp_path, statement, expected):
    compiler = shutil.which('cc')
    if compiler is None:
        pytest.skip('Host C compiler unavailable')
    source = ('int run(int x) { int hits = 0;\n' + statement + '\nreturn hits; }\n'
              + f'int main(void) {{ return run(0) != {expected[0]} || run(1) != {expected[1]}; }}\n')
    result = markers.scrub_text(source)
    assert result.text == source
    for name, text in [('before', source), ('after', result.text)]:
        executable = tmp_path / name
        subprocess.run([compiler, '-std=c11', '-x', 'c', '-', '-o', str(executable)],
                       input=text.encode(), capture_output=True, check=True)
        subprocess.run([str(executable)], capture_output=True, check=True, timeout=5)
    assert markers.scrub_text(result.text).text == result.text


@pytest.mark.parametrize('body,live', [
    ('#if 0\nint dead;\n#\\\nelse\nint live;\n#endif\n', True),
    ('#if 0\nint dead;\n%:else\nint live;\n#endif\n', True),
    ('#if 0\nint dead;\n#elifdef ENABLED\nint live;\n#endif\n', True),
    ('#if 0\nint dead;\n#elifndef ABSENT\nint live;\n#endif\n', True),
    ('#if 0\f|| 1\nint live;\n#endif\n', True),
    ('#if 0\v|| 1\nint live;\n#endif\n', True),
    ('#if 0 /*\n*/ || 1\nint live;\n#endif\n', True),
    ('#if 0\n%:if 1\nint dead;\n%:endif\n#endif\n', False),
    ('#if 0\nint dead;\n#endif\nint live;\n', True),
    ('#if 0\n// continued comment \\\n#endif\nint dead;\n#endif\nint live;\n', True),
])
def test_directives_match_compiler_output(markers, compiler, body, live):
    source = '#define ENABLED\n' + BEGIN + body + END + 'int public_api;\n'
    result = markers.scrub_text(source)
    outputs = []
    for text in (source, result.text):
        outputs.append(subprocess.run([compiler, '-E', '-P', '-std=c2x', '-x', 'c', '-'],
                                     input=text.encode(), capture_output=True, check=True).stdout)
    assert outputs[0].split() == outputs[1].split()
    if '#elifdef' in body or '#elifndef' in body:
        if b'int live;' not in outputs[0]:
            pytest.skip('Compiler does not support C23 elifdef/elifndef')
    assert (b'int live;' in outputs[0]) == live
    assert b'int dead;' not in outputs[0]
    assert b'int public_api;' in outputs[0]
    assert markers.scrub_text(result.text).text == result.text


@pytest.mark.parametrize('prefix,language,flags,reason', [
    ('const char *value = R"tag(\n', 'c++', ['-std=c++17'], 'raw string literal'),
    ('??=define FLAG 1\n', 'c', ['-std=c11', '-trigraphs'], 'trigraph'),
    ('#define FLAG \\ \n1\n', 'c', ['-std=c11'], 'backslash-whitespace-newline'),
])
def test_unsupported_dialects_refuse_before_tree_writes(markers, compiler, tmp_path,
                                                       prefix, language, flags, reason):
    suffix = ')tag";\n' if language == 'c++' else 'int public_api;\n'
    source = prefix + BEGIN + '// literal or comment payload\n' + END + suffix
    subprocess.run([compiler, '-fsyntax-only', '-x', language, *flags, '-'],
                   input=source.encode(), capture_output=True, check=True)
    earlier = tmp_path / 'a.h'
    later = tmp_path / 'z.h'
    dead = BEGIN + '// private note\n' + END
    earlier.write_text(dead)
    later.write_text(source)
    with pytest.raises(markers.InternalMarkerError, match=reason):
        markers.scrub_tree(tmp_path, write=True)
    assert earlier.read_text() == dead
    assert later.read_text() == source


@pytest.mark.parametrize('source', [
    '#define FLAG \\\n' + BEGIN + '// private note\n' + END + 'int public_api;\n',
    '/* prefix\n' + BEGIN + '// private note\n' + END + 'suffix */\nint public_api;\n',
])
def test_partial_logical_line_deletion_is_refused(markers, source):
    with pytest.raises(markers.InternalMarkerError, match='continued preprocessing line'):
        markers.scrub_text(source)


@pytest.mark.parametrize('newline', ['\n', '\r\n', '\r'])
def test_comment_only_deletion_preserves_surviving_raw_bytes(markers, newline):
    source = ('int public_api;' + newline + BEGIN.replace('\n', newline)
              + '// private note' + newline + END.replace('\n', newline) + '\fint tail;')
    result = markers.scrub_text(source)
    assert result.text == 'int public_api;' + newline + '\fint tail;'
    assert markers.scrub_text(result.text).text == result.text


def test_commented_declaration_with_trailing_semicolon_is_retained(markers):
    source = BEGIN + '/* typedef enum { OLD_ENTRY } old_type; */;\n' + END
    assert markers.scrub_text(source).text == source


@pytest.mark.parametrize('prefix', ['#', '%:'])
@pytest.mark.parametrize('keyword', ['if', 'ifdef', 'ifndef', 'endif', 'else', 'elif', 'elifdef', 'elifndef'])
@pytest.mark.parametrize('suffix', [r'\u00E9', r'\U000000E9', '\u00e9', '\u0301', '$'])
def test_ambiguous_conditional_suffixes_are_refused(markers, compiler, prefix, keyword, suffix):
    # Unknown directive names are ignored in skipped groups. They cannot open,
    # close or provide a live alternate for the real conditional around them.
    source = BEGIN + '#if 0\n' + prefix + keyword + suffix + '\nint dead;\n#endif\n' + END + 'int live;\n'
    output = subprocess.run([compiler, '-E', '-P', '-x', 'c', '-'], input=source.encode(),
                            capture_output=True, check=True).stdout.split()
    assert output == [b'int', b'live;']
    with pytest.raises(markers.InternalMarkerError, match='ambiguous conditional directive|non-ASCII code token|universal character name'):
        markers.scrub_text(source)


@pytest.mark.parametrize('suffix', ['\u00a0', '\u2003', '\u0085', r'\q'])
def test_compiler_accepted_else_extra_tokens_refuse_before_writes(markers, compiler, tmp_path, suffix):
    # Both compilers accept these as real #else plus extra tokens (with a
    # warning), rather than an identifier continuation. Never guess and drop it.
    source = BEGIN + '#if 0\nint dead;\n#else' + suffix + '\nint live;\n#endif\n' + END
    output = subprocess.run([compiler, '-E', '-P', '-x', 'c', '-'], input=source.encode(),
                            capture_output=True, check=True).stdout.split()
    assert output == [b'int', b'live;']
    dead = BEGIN + '// private note\n' + END
    (tmp_path / 'a.h').write_text(dead)
    (tmp_path / 'z.h').write_text(source)
    with pytest.raises(markers.InternalMarkerError, match='ambiguous conditional directive|non-ASCII code token|universal character name'):
        markers.scrub_tree(tmp_path, write=True)
    assert (tmp_path / 'a.h').read_text() == dead
    assert (tmp_path / 'z.h').read_text() == source


@pytest.mark.parametrize('suffix', ['_', '0', 'suffix'])
def test_ascii_identifier_suffix_remains_unambiguously_not_a_keyword(markers, compiler, suffix):
    source = BEGIN + '#if 0\n%:endif' + suffix + '\nint dead;\n#endif\n' + END + 'int live;\n'
    result = markers.scrub_text(source)
    assert result.text == 'int live;\n'
    for text in (source, result.text):
        assert subprocess.run([compiler, '-E', '-P', '-x', 'c', '-'], input=text.encode(),
                              capture_output=True, check=True).stdout.split() == [b'int', b'live;']


@pytest.mark.parametrize('number', ["1'000", "0xA'B", "1e1'0", "1'\\\n000", "1e+L'0"])
def test_digit_separators_cannot_hide_live_alternate(markers, compiler, tmp_path, number):
    source = (BEGIN + '#if 0\nint ignored = ' + number + "; // don't /*\n"
              + '#else\nint live;\n/* end */\n#endif\n' + END)
    result = subprocess.run([compiler, '-E', '-P', '-std=c++14', '-x', 'c++', '-'],
                            input=source.encode(), capture_output=True, check=True)
    assert result.stdout.split() == [b'int', b'live;']
    dead = BEGIN + '// private note\n' + END
    (tmp_path / 'a.h').write_text(dead)
    (tmp_path / 'z.h').write_text(source)
    with pytest.raises(markers.InternalMarkerError, match='digit separator'):
        markers.scrub_tree(tmp_path, write=True)
    assert (tmp_path / 'a.h').read_text() == dead
    assert (tmp_path / 'z.h').read_text() == source


@pytest.mark.parametrize('prefix', ['', 'L', 'u', 'U', 'u8'])
def test_standard_character_literal_prefixes_remain_live(markers, compiler, prefix):
    source = BEGIN + "const auto value = " + prefix + "'x';\n" + END
    subprocess.run([compiler, '-fsyntax-only', '-std=c++17', '-x', 'c++', '-'],
                   input=source.encode(), capture_output=True, check=True)
    assert markers.scrub_text(source).text == source


@pytest.mark.parametrize('predecessor', ['\u00a0', '\u2003', '\u00a9', r'\u00A0'])
def test_uncertain_number_predecessor_refuses_before_quote_scanning(markers, tmp_path, predecessor):
    source = (BEGIN + '#if 0\nint ignored = ' + predecessor + "1'000; // don't /*\n"
              + '#else\nint live;\n/* end */\n#endif\n' + END)
    dead = BEGIN + '// private note\n' + END
    (tmp_path / 'a.h').write_text(dead)
    (tmp_path / 'z.h').write_text(source)
    with pytest.raises(markers.InternalMarkerError, match='non-ASCII code token|universal character name'):
        markers.scrub_tree(tmp_path, write=True)
    assert (tmp_path / 'a.h').read_text() == dead
    assert (tmp_path / 'z.h').read_text() == source


def test_unicode_comments_and_literal_contents_stay_supported(markers, compiler):
    source = BEGIN + '// café\nconst char *text = "café \\u00E9";\n' + END
    subprocess.run([compiler, '-fsyntax-only', '-std=c11', '-x', 'c', '-'],
                   input=source.encode(), capture_output=True, check=True)
    assert markers.scrub_text(source).text == source


@pytest.mark.parametrize('quote', ['"', "'"])
@pytest.mark.parametrize('backslashes', [2, 3, 4])
def test_quote_escape_cannot_skip_a_remaining_newline(markers, compiler, tmp_path, quote, backslashes):
    source = (BEGIN + '#if 0\n' + quote + '\\' * backslashes + '\n\n#else // '
              + quote + '\nint live;\n#endif\n' + END)
    result = subprocess.run([compiler, '-E', '-P', '-x', 'c', '-'], input=source.encode(),
                            capture_output=True, check=True)
    assert result.stdout.split() == [b'int', b'live;']
    dead = BEGIN + '// private note\n' + END
    (tmp_path / 'a.h').write_text(dead)
    (tmp_path / 'z.h').write_text(source)
    with pytest.raises(markers.InternalMarkerError, match='newline in an unterminated quoted literal'):
        markers.scrub_tree(tmp_path, write=True)
    assert (tmp_path / 'a.h').read_text() == dead
    assert (tmp_path / 'z.h').read_text() == source
