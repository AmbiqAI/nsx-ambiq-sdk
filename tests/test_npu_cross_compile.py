"""Cross-compile coverage for nsx-npu's C sources.

The CMake contract fixture configures with the host compiler and cannot build
ARM translation units, and the container link smoke covers only the staged
Apollo boards -- so without this test nothing in CI compiles
`modules/nsx-npu/src/**`. It delegates to `tools/nsx_npu_compile_check.py`,
which uses arm-none-eabi-gcc directly with the SoC facts' flags.

Locally the test skips when no toolchain is installed, and covers only
`nsx_npu_timebase.c` (against a generated stub header) unless
`NSX_ETHOS_U_DRIVER_ROOT` points at an `nsx-ethos-u-driver` checkout. The
`arm-compile` job in `.github/workflows/ci.yml` sets `NSX_REQUIRE_ARM_GCC=1`
and `NSX_REQUIRE_ETHOS_U_DRIVER=1`, which turn both the missing-toolchain skip
and a missing driver checkout into failures so neither layer of coverage can
silently lapse in CI.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import pytest

from tools.nsx_npu_compile_check import find_gcc, read_soc_facts


def _gcc_or_skip() -> Path:
    gcc = find_gcc(None)
    if gcc is None:
        message = "arm-none-eabi-gcc not found ($GCC_ROOT/bin or PATH)"
        if os.environ.get("NSX_REQUIRE_ARM_GCC") == "1":
            raise AssertionError(message + "; NSX_REQUIRE_ARM_GCC=1 forbids skipping")
        pytest.skip(message)
    return gcc


def test_soc_facts_expose_compiler_flags(repo_root: Path) -> None:
    # Host-only guard: the compile check derives its flags from the facts file,
    # so a facts reshuffle that stops exposing them must fail here even on a
    # machine without the cross toolchain.
    facts = read_soc_facts(repo_root)
    assert facts["cpu"] == "cortex-m55"
    assert facts["float_abi"] == "hard"
    assert "PART_atomiq110" in facts["definitions"]


def _driver_root_or_skip() -> str | None:
    driver_root = os.environ.get("NSX_ETHOS_U_DRIVER_ROOT")
    if not driver_root and os.environ.get("NSX_REQUIRE_ETHOS_U_DRIVER") == "1":
        raise AssertionError(
            "NSX_ETHOS_U_DRIVER_ROOT unset; NSX_REQUIRE_ETHOS_U_DRIVER=1 forbids skipping "
            "the nsx_npu.c / real-header compile"
        )
    return driver_root


def test_nsx_npu_sources_cross_compile(repo_root: Path, tmp_path: Path) -> None:
    gcc = _gcc_or_skip()
    driver_root = _driver_root_or_skip()
    command = [
        sys.executable,
        str(repo_root / "tools" / "nsx_npu_compile_check.py"),
        "--repo-root",
        str(repo_root),
        "--gcc",
        str(gcc),
        "--build-root",
        str(tmp_path / "npu-compile-check"),
    ]
    if driver_root:
        command += ["--ethos-u-driver-root", driver_root]
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "ok   modules/nsx-npu/src/atomiq110/nsx_npu_timebase.c" in result.stdout
    if driver_root:
        # nsx_npu.c only compiles in the real-driver-header pass; assert it was
        # actually exercised rather than the tool having silently skipped it.
        assert "[real driver headers] skipped" not in result.stdout
        assert "ok   modules/nsx-npu/src/nsx_npu.c" in result.stdout
