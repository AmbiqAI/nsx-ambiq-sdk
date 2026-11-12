"""Exercise policy boundaries against the actual DMA helper, without hardware."""
import subprocess
from pathlib import Path

import pytest


def compile_policy(repo_root: Path, binary: Path, qualified: bool) -> None:
    module = repo_root / 'modules/nsx-psram'
    fixture = repo_root / 'tests/fixtures'
    subprocess.run([
        'cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fno-pie', '-no-pie',
        f'-DNSX_PSRAM_ASYNC_BURST_QUALIFIED={int(qualified)}',
        f'-I{fixture / "psram-dma"}', f'-I{module / "includes-api"}',
        f'-I{module / "src"}', f'-I{module / "src/apollo5"}',
        str(module / 'src/apollo5/nsx_psram_dma.c'), str(fixture / 'psram-policy/main.c'),
        '-o', str(binary),
    ], check=True)


@pytest.fixture(scope='module')
def policy_binary(repo_root: Path, tmp_path_factory: pytest.TempPathFactory) -> Path:
    binary = tmp_path_factory.mktemp('psram-policy') / 'test'
    compile_policy(repo_root, binary, True)
    return binary


@pytest.mark.parametrize('scenario', ['switching', 'refusals', 'clock-gate'])
def test_psram_receive_policy(policy_binary: Path, scenario: str) -> None:
    subprocess.run([str(policy_binary), scenario], check=True, timeout=10)


def test_psram_unqualified_policy(repo_root: Path, tmp_path: Path) -> None:
    binary = tmp_path / 'test'
    compile_policy(repo_root, binary, False)
    subprocess.run([str(binary), 'unqualified'], check=True, timeout=10)
