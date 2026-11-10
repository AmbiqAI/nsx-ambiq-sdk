from __future__ import annotations

import subprocess
from pathlib import Path

import pytest


@pytest.fixture(scope="module")
def psram_dma_binary(repo_root: Path, tmp_path_factory: pytest.TempPathFactory) -> Path:
    binary = tmp_path_factory.mktemp("psram-dma") / "test"
    _compile(repo_root, binary)
    return binary


def _compile(repo_root: Path, binary: Path, *defines: str) -> None:
    module = repo_root / "modules/nsx-psram"
    fixture = repo_root / "tests/fixtures/psram-dma"
    subprocess.run(
        [
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fno-pie", "-no-pie",
            *defines, f"-I{fixture}", f"-I{module / 'includes-api'}",
            f"-I{module / 'src'}", f"-I{module / 'src/apollo5'}",
            str(module / "src/apollo5/nsx_psram_dma.c"), str(fixture / "main.c"),
            "-o", str(binary),
        ], check=True,
    )


@pytest.mark.parametrize("scenario", [
    "placement", "success", "alignment", "clocks", "error",
    "timeout-read", "timeout-write", "masked",
    "later-read-error", "later-write-error", "later-timeout",
])
def test_psram_dma_faults(psram_dma_binary: Path, scenario: str) -> None:
    subprocess.run([str(psram_dma_binary), scenario], check=True, timeout=10)


def test_psram_dma_rejects_bad_placement(repo_root: Path, tmp_path: Path) -> None:
    binary = tmp_path / "bad-placement"
    _compile(repo_root, binary, "-DDTCM_BASEADDR=0x20000000u", "-DEXPECT_BAD_PLACEMENT")
    subprocess.run([str(binary), "placement"], check=True, timeout=10)
