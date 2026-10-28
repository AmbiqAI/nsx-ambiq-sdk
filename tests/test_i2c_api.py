"""Execute the real NSX I2C wrapper against a minimal recording HAL boundary."""
from __future__ import annotations

import subprocess
from pathlib import Path

import pytest


@pytest.mark.parametrize("part", ["AM_PART_APOLLO3", "AM_PART_APOLLO5A"])
@pytest.mark.parametrize("validation", [True, False])
def test_combined_i2c_contract(repo_root: Path, tmp_path: Path, part: str, validation: bool) -> None:
    include = repo_root / "modules/nsx-i2c/includes-api"
    fixtures = repo_root / "tests/i2c_host"
    executable = tmp_path / "i2c_contract"
    flags = [] if validation else ["-DAM_HAL_DISABLE_API_VALIDATION", "-DNSX_DISABLE_API_VALIDATION"]
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
        f"-D{part}", *flags, "-I", str(fixtures), "-I", str(include),
        str(repo_root / "modules/nsx-i2c/src/nsx_i2c.c"),
        str(fixtures / "test_i2c.c"), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
