"""Apollo4 HEX DDR refusals stay retryable; actual transfer errors do not."""
from __future__ import annotations

import subprocess
from pathlib import Path

import pytest


def _build(repo: Path, tmp: Path, mode: int, common: Path | None = None,
           *, original: bool = False) -> Path:
    module = repo / 'modules/nsx-psram'
    # Compile the actual platform validator against the common implementation.
    # Other HAL/device entry points are isolated because no peripheral is used.
    platform = (module / 'src/apollo4/nsx_psram.c').read_text()
    start = platform.index('uint32_t nsx_psram_platform_validate_blocking(')
    end = platform.index('\n}', start) + 2
    validator = tmp / 'validator.c'
    validator.write_text('#include "nsx_psram_internal.h"\n' + platform[start:end] + '\n')
    binary = tmp / 'test'
    subprocess.run([
        'cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
        '-DNSX_PSRAM_PLATFORM_CAPABILITIES=7u',
        '-DAM_HAL_MSPI_FLASH_HEX_DDR_CE0=1', '-DAM_HAL_MSPI_FLASH_HEX_DDR_CE1=2',
        f'-DNSX_PSRAM_DEVICE_CONFIG={mode}',
        *(['-DEXPECT_OLD_REGRESSION'] if original else []),
        f'-I{repo / "tests/fixtures/psram-async"}',
        f'-I{module / "includes-api"}', f'-I{module / "src"}',
        str(common or module / 'src/nsx_psram.c'), str(validator),
        str(repo / 'tests/fixtures/psram-ap4/main.c'), '-o', str(binary),
    ], check=True)
    return binary


@pytest.mark.parametrize('mode', [1, 2, 3])  # HEX CE0/CE1 and an unconstrained mode.
def test_ap4_alignment_refusal_preserves_ready(repo_root: Path, tmp_path: Path, mode: int) -> None:
    subprocess.run([str(_build(repo_root, tmp_path, mode))], check=True, timeout=10)


def test_ap4_model_rejects_async_gated_validation(repo_root: Path, tmp_path: Path) -> None:
    source = (repo_root / 'modules/nsx-psram/src/nsx_psram.c').read_text()
    needle = 'status = nsx_psram_platform_validate_blocking(offset, length, write);'
    assert source.count(needle) == 1
    mutant = tmp_path / 'async-gated.c'
    mutant.write_text(source.replace(needle,
        'if (g_nsx_psram_info.capabilities & NSX_PSRAM_CAP_ASYNC_READ) ' + needle))
    result = subprocess.run([str(_build(repo_root, tmp_path, 1, mutant))],
                            capture_output=True, timeout=10)
    assert result.returncode != 0
