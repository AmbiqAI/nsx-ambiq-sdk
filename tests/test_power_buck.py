"""Exercise the real session lifecycle with deterministic timer and timebase inputs."""

import subprocess


def test_buck_measurement_session(repo_root, tmp_path):
    power = repo_root / "modules/nsx-power"
    binary = tmp_path / "buck-session"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(power / "includes-api"), "-I", str(power / "src"),
        str(power / "src/nsx_power_buck.c"),
        str(repo_root / "tests/fixtures/power-buck/session.c"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
