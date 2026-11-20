"""Exercise TIMER ownership and hardware status decoding with each released register map."""

import subprocess

import pytest


@pytest.mark.parametrize("soc", ["apollo4l", "apollo4p", "apollo510"])
def test_buck_timer_backend(repo_root, tmp_path, soc):
    sdk = repo_root / "modules/nsx-ambiqsuite/sdk"
    power = repo_root / "modules/nsx-power"
    fixture = repo_root / "tests/fixtures/buck-timer"
    binary = tmp_path / "buck-timer"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", f"-DAM_PART_{soc.upper()}",
        f'-DTEST_DEVICE="{soc}.h"', f"-I{fixture}", f"-I{sdk / 'mcu' / soc}",
        f"-I{sdk / 'CMSIS/AmbiqMicro/Include'}", f"-I{power / 'includes-api'}",
        f"-I{power / 'src'}", str(fixture / "main.c"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
