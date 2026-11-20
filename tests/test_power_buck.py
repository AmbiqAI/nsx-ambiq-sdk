"""Exercise the real session lifecycle with deterministic timer and timebase inputs."""

import subprocess


def test_buck_measurement_session(repo_root, tmp_path):
    power = repo_root / "modules/nsx-power"
    fixture = repo_root / "tests/fixtures/power-buck"
    binary = tmp_path / "buck-session"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(power / "includes-api"), "-I", str(power / "src"),
        "-I", str(fixture),
        str(power / "src/nsx_power_buck.c"),
        str(fixture / "session.c"),
        "-o", str(binary),
    ], check=True)
    for initial_primask in ("0", "1"):
        subprocess.run([str(binary), initial_primask], check=True)
