"""Build the SDK-owned test application against shipped CMake targets."""
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

from .protocol import RESULT_SIZE, validate_cases
from .suites import Suite
from .preparation import MemorySeed

from .inventory import BOARDS
from .provenance import checkout, dependency_inputs, cmake_inputs, classify_inputs


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(args: list[str], log: Path, timeout: float = 180) -> str:
    result = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    with log.open('a') as stream:
        stream.write(f'{args!r}\n{result.stdout}\n{result.stderr}\n')
    if result.returncode:
        raise RuntimeError(f'Command failed ({result.returncode}); see {log}')
    return result.stdout


@dataclass(frozen=True)
class Firmware:
    elf: Path
    address: int
    build_id: str
    board: str
    device: str
    speed: int
    cases: tuple[str, ...]
    seeds: tuple[MemorySeed, ...] = ()

    def __post_init__(self):
        validate_cases(self.cases)


def build(repo: Path, output: Path, board: str, gcc_root: Path, suite: Suite, fault: str = "none", *, sdk_root: Path | None = None) -> Firmware:
    sdk_root = sdk_root or repo
    for root in (repo, sdk_root):
        if not root.is_dir():
            raise ValueError(f"Missing checkout: {root}")
    if board not in BOARDS:
        raise ValueError(f'Unsupported PoC board: {board}')
    if output.exists() and any(output.iterdir()):
        raise ValueError('Firmware output must be a new or empty directory')
    output.mkdir(parents=True, exist_ok=True)
    log = output / 'build.log'
    query = output / '.cmake/api/v1/query/cmakeFiles-v1'
    query.parent.mkdir(parents=True)
    query.touch()
    cc = gcc_root / 'bin/arm-none-eabi-gcc'
    cxx = gcc_root / 'bin/arm-none-eabi-g++'
    build_id = uuid.uuid4().hex
    (output / 'identity.h').write_text(f'#define NSX_TEST_BUILD_ID "{build_id}"\n')
    if not suite.source_dir:
        raise ValueError('Suite requires a firmware source directory')
    extra = suite.configure(repo, output, board, fault, sdk_root)
    command(['cmake', '-S', str(repo / suite.source_dir), '-B', str(output),
             '-G', 'Ninja', '-DCMAKE_SYSTEM_NAME=Generic', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
             '-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY',
             f'-DCMAKE_C_COMPILER={cc}', f'-DCMAKE_CXX_COMPILER={cxx}',
             f'-DCMAKE_ASM_COMPILER={cc}', f'-DNSX_TEST_SDK_ROOT={sdk_root}',
             f'-DNSX_TEST_HARNESS_ROOT={repo}',
             f'-DNSX_TEST_BOARD={board}', f'-DNSX_TEST_FAULT={fault}', '-DCMAKE_BUILD_TYPE=Debug'] + extra, log)
    command(['cmake', '--build', str(output)], log)
    elf = output / suite.elf_name
    symbols = command([str(gcc_root / 'bin/arm-none-eabi-nm'), '-S', str(elf)], log)
    symbol = [line.split() for line in symbols.splitlines() if line.endswith(' nsx_test_result')]
    if len(symbol) != 1 or int(symbol[0][1], 16) != RESULT_SIZE:
        raise RuntimeError('Result symbol missing or protocol layout differs')
    seeds = []
    for seed in suite.seeds:
        name = seed.symbol
        matches = [line.split() for line in symbols.splitlines() if line.endswith(' ' + name)]
        if len(matches) != 1 or int(matches[0][1], 16) != 4:
            raise RuntimeError(f'Missing or malformed suite seed {name}')
        seeds.append(MemorySeed(name, int(matches[0][0], 16), seed.value))
    inputs = [Path(p) for p in (output / 'inputs.txt').read_text().splitlines()]
    sources = [p for root in (repo / suite.source_dir, repo / 'tests/hardware/common')
               for p in root.rglob('*') if p.is_file()]
    compiled = [Path(entry['file']) for entry in json.loads((output / 'compile_commands.json').read_text())]
    dependencies = dependency_inputs(command(['ninja', '-C', str(output), '-t', 'deps'], log), output)
    sources = sorted(set(sources + compiled + dependencies + cmake_inputs(output)))
    sources += list((repo / 'tools/hardware').glob('*.py'))
    vendor = list((repo / 'tests/hardware/vendor/unity').glob('*'))
    device, speed = (output / 'probe.txt').read_text().splitlines()
    if not device or int(speed) <= 0:
        raise RuntimeError('Board descriptor lacks valid SEGGER defaults')
    manifest = {
        'schema_version': 3, 'board': board, 'build_id': build_id, 'fault_injection': fault,
        'harness': checkout(repo),
        'sdk': checkout(sdk_root),
        'device': device, 'swd_speed_khz': int(speed),
        'seeds': [dict(symbol=s.symbol, address=s.address, value=s.value) for s in seeds],
        'expected_cases': suite.cases,
        'compiler': command([str(cc), '--version'], log).splitlines()[0],
        'elf_sha256': digest(elf),
        'suite': suite.name, 'toolchain': 'gcc',
        'sdk_identity_sha256': {str(p.relative_to(sdk_root)): digest(p) for p in (
            sdk_root / 'modules/nsx-ambiqsuite/nsx-module.yaml',
            sdk_root / 'modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml',
            sdk_root / 'modules/nsx-ambiqsuite/sdk/mcu/am_sdk_version.h')},
        'inputs_sha256': {str(p): digest(p) for p in inputs + sources + vendor},
        'input_roles': classify_inputs(inputs + sources + vendor, repo, sdk_root, output),
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return Firmware(elf, int(symbol[0][0], 16), build_id, board, device, int(speed), suite.cases, tuple(seeds))
