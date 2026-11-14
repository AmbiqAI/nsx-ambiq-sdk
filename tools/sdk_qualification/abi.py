"""Narrow GCC enum/wchar/archive ABI-policy proof; no vendor MMIO executes."""
from dataclasses import replace
import json
from pathlib import Path
import re
import shlex

from tools.hardware.suites import Suite
from .io import contained, read_json, sha256
from .runtime import Tools

SYMBOLS = ('nsx_test_abi_c', 'nsx_test_abi_cpp', 'am_hal_pdm_configure', 'am_bsp_low_power_init')
EXPECTED = {'Tag_ABI_PCS_wchar_t': '4', 'Tag_ABI_enum_size': 'small',
            'Tag_ABI_VFP_args': 'VFP registers'}


def enabled_suite(suite: Suite) -> Suite:
    def configure(repo, output, board, fault, sdk_root=None):
        return [*suite.configure(repo, output, board, fault, sdk_root), '-DNSX_TEST_ABI_POLICY=ON']
    return replace(suite, configure=configure)


def attributes(text: str) -> dict[str, list[str]]:
    observed = {name: sorted(set(re.findall(r'^\s*' + name + r':\s*(.+?)\s*$', text, re.MULTILINE)))
                for name in EXPECTED}
    if any(observed[name] != [expected] for name, expected in EXPECTED.items()):
        raise ValueError(f'GCC ABI policy mismatch: {observed}')
    return observed


def capture(output: Path, harness: Path, tools: Tools, command) -> dict:
    entries = json.loads((output / 'compile_commands.json').read_text())
    objects = []
    for source, compiler in (('abi_policy.c', 'gcc'), ('abi_policy.cpp', 'g++')):
        expected = (harness / 'tests/hardware/common' / source).resolve()
        selected = [entry for entry in entries if Path(entry['file']).resolve() == expected]
        if len(selected) != 1:
            raise ValueError(f'ABI probe must compile exactly once: {source}')
        args = shlex.split(selected[0]['command'])
        if Path(args[0]).resolve() != tools.path(compiler).resolve():
            raise ValueError('ABI probe used a different compiler')
        obj = (Path(selected[0]['directory']) / args[args.index('-o') + 1]).resolve()
        if not obj.is_relative_to(output.resolve()) or not obj.is_file():
            raise ValueError('ABI object is outside its build')
        objects.append(obj)
    inputs = (output / 'inputs.txt').read_text().splitlines()
    if len(inputs) != 5:
        raise ValueError('Expected exact startup/system/linker/HAL/BSP inputs')
    archives = [Path(path) for path in inputs[-2:]]
    linked = output / 'abi-linked.o'
    command([str(tools.path('ld')), '-r', '--fatal-warnings', '-z', 'noexecstack',
             '-u', SYMBOLS[0], '-u', SYMBOLS[1], *(str(p) for p in objects + archives), '-o', str(linked)],
            'abi-link')
    symbol_text = command([str(tools.path('nm')), '-g', '--defined-only', str(linked)], 'abi-symbols')
    for name in SYMBOLS:
        if not re.search(r'\b[TW]\s+' + re.escape(name) + '$', symbol_text, re.MULTILINE):
            raise ValueError(f'Missing force-linked ABI probe/archive definition: {name}')
    observed = {}
    for index, path in enumerate([*objects, linked, *archives]):
        observed[str(path)] = attributes(command([str(tools.path('readelf')), '-A', str(path)], f'abi-attributes-{index}'))
    proof = {'schema_version': 1, 'claim': 'gcc-enum-wchar-archive-policy',
             'objects': [str(p.relative_to(output)) for p in objects],
             'linked': linked.name, 'archives': [str(p) for p in archives],
             'artifacts_sha256': {str(p.relative_to(output)): sha256(p)
                                  for p in [*objects, linked, output / 'compile_commands.json', output / 'inputs.txt']},
             'attributes': observed}
    (output / 'abi.json').write_text(json.dumps(proof, indent=2) + '\n')
    return proof


def verify(output: Path, tools: Tools, archives: list[Path], command) -> None:
    proof = read_json(output / 'abi.json')
    if proof['schema_version'] != 1 or proof['claim'] != 'gcc-enum-wchar-archive-policy':
        raise ValueError('Unsupported ABI proof')
    if set(proof['artifacts_sha256']) != {*proof['objects'], proof['linked'], 'compile_commands.json', 'inputs.txt'}:
        raise ValueError('Incomplete ABI artifact inventory')
    if len(proof['objects']) != 2 or len(set(proof['objects'])) != 2 or proof['linked'] != 'abi-linked.o':
        raise ValueError('Expected two language probes and their ABI link')
    for relative, expected in proof['artifacts_sha256'].items():
        if sha256(contained(output, relative)) != expected:
            raise ValueError(f'ABI artifact changed: {relative}')
    paths = [contained(output, name) for name in [*proof['objects'], proof['linked']]]
    for index, path in enumerate([*paths, *archives]):
        attributes(command([str(tools.path('readelf')), '-A', str(path)], f'verify-abi-{index}'))
    symbols = command([str(tools.path('nm')), '-g', '--defined-only', str(paths[-1])], 'verify-abi-symbols')
    if any(not re.search(r'\b[TW]\s+' + re.escape(name) + '$', symbols, re.MULTILINE) for name in SYMBOLS):
        raise ValueError('ABI linked definitions changed')
