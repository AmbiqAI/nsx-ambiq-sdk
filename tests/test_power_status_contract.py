"""Run the live snapshot against distinct register values and injected HAL failures."""

import subprocess

import pytest


@pytest.mark.parametrize("defines", [
    ["-DNSX_POWER_STATUS_HAL"],
    ["-DNSX_POWER_STATUS_APOLLO3"],
    ["-DPART_apollo5b", "-DAM_PART_APOLLO510"],  # Legacy Apollo5b compatibility macro alone enables no backend.
])
def test_power_status(repo_root, tmp_path, defines):
    power = repo_root / "modules/nsx-power"
    fixture = repo_root / "tests/fixtures/power-status"
    binary = tmp_path / "power-status"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", *defines,
        "-I", str(power / "includes-api"), "-I", str(fixture),
        str(power / "src/nsx_power_status.c"), str(fixture / "main.c"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
