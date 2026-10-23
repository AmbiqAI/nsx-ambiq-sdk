"""Build the SDK-owned test application against shipped CMake targets."""
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

from .protocol import RESULT_SIZE
from .startup import SENTINELS, OMISSIONS, make_mutant

from .inventory import BOARDS


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
    sentinels: tuple[int, ...] = ()


def build(repo: Path, output: Path, board: str, gcc_root: Path, fault: str = "none") -> Firmware:
    if board not in BOARDS:
        raise ValueError(f'Unsupported PoC board: {board}')
    output.mkdir(parents=True, exist_ok=True)
    log = output / 'build.log'
    cc = gcc_root / 'bin/arm-none-eabi-gcc'
    cxx = gcc_root / 'bin/arm-none-eabi-g++'
    build_id = uuid.uuid4().hex
    (output / 'identity.h').write_text(f'#define NSX_TEST_BUILD_ID "{build_id}"\n')
    extra = []
    if fault in OMISSIONS:
        extra = [f'-DNSX_TEST_STARTUP_OVERRIDE={make_mutant(repo, output, board, fault)}']
    command(['cmake', '-S', str(repo / 'tests/hardware/firmware'), '-B', str(output),
             '-G', 'Ninja', '-DCMAKE_SYSTEM_NAME=Generic',
             '-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY',
             f'-DCMAKE_C_COMPILER={cc}', f'-DCMAKE_CXX_COMPILER={cxx}',
             f'-DCMAKE_ASM_COMPILER={cc}', f'-DNSX_TEST_SDK_ROOT={repo}',
             f'-DNSX_TEST_BOARD={board}', f'-DNSX_TEST_FAULT={fault}', '-DCMAKE_BUILD_TYPE=Debug'] + extra, log)
    command(['cmake', '--build', str(output)], log)
    elf = output / 'startup_test.elf'
    symbols = command([str(gcc_root / 'bin/arm-none-eabi-nm'), '-S', str(elf)], log)
    symbol = [line.split() for line in symbols.splitlines() if line.endswith(' nsx_test_result')]
    if len(symbol) != 1 or int(symbol[0][1], 16) != RESULT_SIZE:
        raise RuntimeError('Result symbol missing or protocol layout differs')
    sentinel_addresses = []
    for name in SENTINELS:
        matches = [line.split() for line in symbols.splitlines() if line.endswith(' ' + name)]
        if len(matches) != 1 or int(matches[0][1], 16) != 4:
            raise RuntimeError(f'Missing or malformed startup sentinel {name}')
        sentinel_addresses.append(int(matches[0][0], 16))
    inputs = [Path(p) for p in (output / 'inputs.txt').read_text().splitlines()]
    sources = list((repo / 'tests/hardware/firmware').glob('*'))
    vendor = list((repo / 'tests/hardware/vendor/unity').glob('*'))
    device, speed = (output / 'probe.txt').read_text().splitlines()
    if not device or int(speed) <= 0:
        raise RuntimeError('Board descriptor lacks valid SEGGER defaults')
    manifest = {
        'schema_version': 1, 'board': board, 'build_id': build_id, 'fault_injection': fault,
        'sdk_revision': command(['git', '-C', str(repo), 'rev-parse', 'HEAD'], log).strip(),
        'sdk_dirty': bool(command(['git', '-C', str(repo), 'status', '--porcelain'], log).strip()),
        'device': device, 'swd_speed_khz': int(speed),
        'sentinels': dict(zip(SENTINELS, sentinel_addresses)),
        'compiler': command([str(cc), '--version'], log).splitlines()[0],
        'elf_sha256': digest(elf),
        'suite': 'startup', 'toolchain': 'gcc',
        'sdk_identity_sha256': {str(p.relative_to(repo)): digest(p) for p in (
            repo / 'modules/nsx-ambiqsuite/nsx-module.yaml',
            repo / 'modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml',
            repo / 'modules/nsx-ambiqsuite/sdk/mcu/am_sdk_version.h')},
        'inputs_sha256': {str(p): digest(p) for p in inputs + sources + vendor},
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return Firmware(elf, int(symbol[0][0], 16), build_id, board, device, int(speed), tuple(sentinel_addresses))
