"""Bounded intake sanitation must preserve reviewed evidence and fail before writes."""
from __future__ import annotations

import hashlib
import importlib.util
import json
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

BEGIN = b"// #### INTERNAL BEGIN ####\n"
END = b"// #### INTERNAL END ####\n"
DEAD = BEGIN + b"// private note\n" + END
NULL_STATEMENT = b"if (x)\n" + BEGIN + b";\n" + END + b"    work();\n"
NULL_OUTPUT = b"if (x)\n    work();\n"
COMMIT = "a" * 40
PREFIX = "modules/nsx-ambiqsuite/sdk"


@pytest.fixture()
def markers(repo_root: Path):
    spec = importlib.util.spec_from_file_location("reviewed_sanitation_markers", repo_root / "sdk-intake/internal_markers.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


@pytest.fixture()
def env(tmp_path: Path, markers):
    root = tmp_path / "staged"
    reference = tmp_path / "reviewed"
    root.mkdir()
    reference.mkdir()
    baseline = tmp_path / "baseline.yaml"
    baseline.write_text(markers.render_baseline({}, [f"{PREFIX}/z.h"]))
    receipt = tmp_path / "replay.json"
    receipt.write_text(json.dumps({"schema_version": 1, "source_commit": COMMIT, "files": {}}))
    environment = (root, reference, baseline, receipt)
    record(markers, environment, DEAD, b"", name="a.h")
    reference.joinpath("a.h").write_bytes(b"")
    return environment


def run(markers, env, *, source_commit=COMMIT):
    root, reference, baseline, receipt = env
    return markers.scrub_reviewed_tree(root, reviewed_root=reference, path_prefix=PREFIX,
                                      source_commit=source_commit, replay_path=receipt, baseline=baseline)


def record(markers, env, before=NULL_STATEMENT, after=NULL_OUTPUT, *, name="z.h"):
    def digest(data):
        return hashlib.sha256(markers.normalize_reviewed_banner(data)).hexdigest()

    receipt = json.loads(env[3].read_text())
    receipt["files"][name] = {"input_sha256": digest(before), "output_sha256": digest(after)}
    env[3].write_text(json.dumps(receipt))


def banner(version: str) -> bytes:
    return f"// This is part of revision {version} of the AmbiqSuite Development Package.\n".encode()


def test_preserves_reviewed_malformed_file_and_reports_original_hash(markers, env):
    before = banner("new-version") + BEGIN
    env[0].joinpath("z.h").write_bytes(before)
    env[1].joinpath("z.h").write_bytes(banner("old-version") + BEGIN)
    result = run(markers, env)
    assert env[0].joinpath("z.h").read_bytes() == before
    assert not result.changed_files
    assert len(result.preserved_files) == 1
    retained = result.preserved_files[0]
    assert retained.path == f"{PREFIX}/z.h"
    assert retained.sha256 == hashlib.sha256(before).hexdigest()
    assert "without a matching END" in retained.reason
    with pytest.raises(markers.InternalMarkerError):
        markers.scrub_tree(env[0], write=True)


@pytest.mark.parametrize("bad", [BEGIN, END, BEGIN + BEGIN + END + END,
                                b"int x; " + BEGIN, b"/* " + BEGIN])
def test_new_malformed_shape_fails_before_any_write(markers, env, bad):
    env[0].joinpath("a.h").write_bytes(DEAD)
    env[0].joinpath("unknown.h").write_bytes(bad)
    with pytest.raises(markers.InternalMarkerError):
        run(markers, env)
    assert env[0].joinpath("a.h").read_bytes() == DEAD
    assert env[0].joinpath("unknown.h").read_bytes() == bad


def test_known_malformed_content_drift_fails_before_writes(markers, env):
    env[0].joinpath("a.h").write_bytes(DEAD)
    env[0].joinpath("z.h").write_bytes(BEGIN + b"// changed\n")
    env[1].joinpath("z.h").write_bytes(BEGIN)
    with pytest.raises(markers.InternalMarkerError, match="differs from reviewed"):
        run(markers, env)
    assert env[0].joinpath("a.h").read_bytes() == DEAD


@pytest.mark.parametrize("shape", ["missing", "file", "root", "ancestor"])
def test_missing_or_symlinked_reference_is_refused(markers, env, tmp_path, shape):
    env[0].joinpath("z.h").write_bytes(BEGIN)
    reference = env[1]
    if shape == "file":
        target = tmp_path / "target.h"
        target.write_bytes(BEGIN)
        reference.joinpath("z.h").symlink_to(target)
    elif shape in {"root", "ancestor"}:
        reference.joinpath("z.h").write_bytes(BEGIN)
        link = tmp_path / "link"
        link.symlink_to(reference, target_is_directory=True)
        if shape == "root":
            env = (env[0], link, env[2], env[3])
        else:
            child = reference / "child"
            child.mkdir()
            child.joinpath("z.h").write_bytes(BEGIN)
            env = (env[0], link / "child", env[2], env[3])
    with pytest.raises(markers.InternalMarkerError, match="reviewed reference"):
        run(markers, env)


@pytest.mark.parametrize("location", ["staged", "reference"])
def test_invalid_utf8_is_never_exempted(markers, env, location):
    env[0].joinpath("z.h").write_bytes(BEGIN)
    env[1].joinpath("z.h").write_bytes(BEGIN)
    env[0 if location == "staged" else 1].joinpath("z.h").write_bytes(BEGIN + b"\xff")
    with pytest.raises(markers.InternalMarkerError, match="not valid UTF-8"):
        run(markers, env)


def test_receipted_semicolon_replay_accepts_banner_drift_and_is_idempotent(markers, env):
    before = banner("current") + NULL_STATEMENT
    after = banner("current") + NULL_OUTPUT
    record(markers, env, banner("historical") + NULL_STATEMENT, banner("historical") + NULL_OUTPUT)
    env[0].joinpath("z.h").write_bytes(before)
    env[1].joinpath("z.h").write_bytes(banner("released") + NULL_OUTPUT)
    result = run(markers, env)
    assert result.replayed_files == (f"{PREFIX}/z.h",)
    assert result.changed_files == ("z.h",)
    assert result.removed_lines == 3
    assert env[0].joinpath("z.h").read_bytes() == after
    repeated = run(markers, env)
    assert not repeated.changed_files
    assert not repeated.replayed_files
    assert env[0].joinpath("z.h").read_bytes() == after


@pytest.mark.parametrize("guard", ["input", "output", "source", "missing_entry", "provider"])
def test_semicolon_replay_guards_fail_before_any_write(markers, env, guard):
    env[0].joinpath("a.h").write_bytes(DEAD)
    before = NULL_STATEMENT
    record(markers, env)
    if guard == "input":
        # The extra comment is removed too: output equality cannot authorize this input.
        before = NULL_STATEMENT.replace(b";\n", b";\n// new input\n", 1)
        assert markers.scrub_text(before.decode()).text.encode() == NULL_OUTPUT
    elif guard == "output":
        record(markers, env, after=b"different output\n")
    elif guard == "missing_entry":
        receipt = json.loads(env[3].read_text())
        del receipt["files"]["z.h"]
        env[3].write_text(json.dumps(receipt))
    env[0].joinpath("z.h").write_bytes(before)
    env[1].joinpath("z.h").write_bytes(b"different provider\n" if guard == "provider" else NULL_OUTPUT)
    with pytest.raises(markers.InternalMarkerError, match="unreviewed sanitation|sanitation replay output"):
        run(markers, env, source_commit="b" * 40 if guard == "source" else COMMIT)
    assert env[0].joinpath("a.h").read_bytes() == DEAD
    assert env[0].joinpath("z.h").read_bytes() == before


def test_preprocessor_dead_semicolon_still_requires_receipt(markers, env):
    before = BEGIN + b"#if 0\n;\n#endif\n" + END
    env[0].joinpath("z.h").write_bytes(before)
    with pytest.raises(markers.InternalMarkerError, match="unreviewed sanitation"):
        run(markers, env)
    assert env[0].joinpath("z.h").read_bytes() == before


def test_normal_comment_removal_requires_receipt(markers, env):
    env[0].joinpath("z.h").write_bytes(DEAD)
    env[1].joinpath("z.h").write_bytes(b"")
    with pytest.raises(markers.InternalMarkerError, match="unreviewed sanitation"):
        run(markers, env)
    assert env[0].joinpath("z.h").read_bytes() == DEAD


def test_receipted_normal_comment_removal_is_idempotent(markers, env):
    record(markers, env, DEAD, b"")
    env[0].joinpath("z.h").write_bytes(DEAD)
    env[1].joinpath("z.h").write_bytes(b"")
    result = run(markers, env)
    assert result.changed_files == ("z.h",)
    assert result.replayed_files == (f"{PREFIX}/z.h",)
    assert env[0].joinpath("z.h").read_bytes() == b""
    assert not run(markers, env).changed_files


SPLICED_ELSE = BEGIN + b"#if 0\nint dead;\n#\\\nelse\nint live;\n#endif\n" + END
SPLICED_COMMENT = (BEGIN + b"#if 0\n// continued comment \\\n#endif\nint inactive;\n"
                   + END + b"#endif\nint public_api;\n")


SPLICED_MACRO = b'#define FLAG \\\n' + DEAD + b"int public_api(void);\n"


@pytest.mark.parametrize("before", [SPLICED_ELSE, SPLICED_COMMENT, SPLICED_MACRO])
def test_unreviewed_line_splicing_is_refused_before_any_write(markers, env, before):
    env[0].joinpath("a.h").write_bytes(DEAD)
    env[0].joinpath("z.h").write_bytes(before)
    with pytest.raises(markers.InternalMarkerError, match="unreviewed sanitation"):
        run(markers, env)
    assert env[0].joinpath("a.h").read_bytes() == DEAD
    assert env[0].joinpath("z.h").read_bytes() == before


@pytest.mark.parametrize("before,live", [(SPLICED_ELSE, b"int live;"),
                                         (SPLICED_COMMENT, b"int public_api;"),
                                         (SPLICED_MACRO, b"int public_api(void);")])
def test_splicing_fixtures_are_valid_c_with_live_declarations(before, live):
    compiler = shutil.which("cc")
    if compiler is None:
        pytest.skip("C compiler unavailable")
    result = subprocess.run([compiler, "-E", "-P", "-x", "c", "-"], input=before,
                            capture_output=True, check=True)
    assert live in result.stdout
    assert b"int dead;" not in result.stdout
    assert b"int inactive;" not in result.stdout


@pytest.mark.parametrize("newline", [b"\n", b"\r\n", b""])
def test_banner_normalization_changes_only_exact_standalone_version(markers, newline):
    old = banner("old-1").rstrip(b"\n") + newline
    new = banner("new+2").rstrip(b"\n") + newline
    assert markers.normalize_reviewed_banner(old) == markers.normalize_reviewed_banner(new)
    assert markers.normalize_reviewed_banner(old).endswith(newline)
    for prefix, suffix in [(b" ", b""), (b"int x; ", b""), (b"", b" trailing")]:
        line = prefix + old.rstrip(b"\r\n") + suffix + newline
        assert markers.normalize_reviewed_banner(line) == line
    unrelated = b'const char *version = "old-1";\n'
    assert markers.normalize_reviewed_banner(unrelated) == unrelated


@pytest.mark.parametrize("mutation", ["schema", "commit", "path", "hash", "extra", "duplicate"])
def test_replay_loader_rejects_invalid_or_ambiguous_receipts(markers, env, mutation):
    record(markers, env)
    receipt = json.loads(env[3].read_text())
    if mutation == "schema":
        receipt["schema_version"] = True
    elif mutation == "commit":
        receipt["source_commit"] = "main"
    elif mutation == "path":
        receipt["files"]["../z.h"] = receipt["files"].pop("z.h")
    elif mutation == "hash":
        receipt["files"]["z.h"]["input_sha256"] = "not a hash"
    elif mutation == "extra":
        receipt["auto_approve"] = True
    encoded = json.dumps(receipt)
    if mutation == "duplicate":
        encoded = encoded.replace('"schema_version": 1', '"schema_version": 1, "schema_version": 1')
    env[3].write_text(encoded)
    with pytest.raises(markers.InternalMarkerError, match="invalid sanitation replay receipt"):
        markers.load_sanitation_replay(env[3])


def test_replay_loader_reports_missing_receipt(markers, tmp_path):
    with pytest.raises(markers.InternalMarkerError, match="invalid sanitation replay receipt"):
        markers.load_sanitation_replay(tmp_path / "missing.json")
