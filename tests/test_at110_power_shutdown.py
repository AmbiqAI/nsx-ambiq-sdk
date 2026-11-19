"""AT110 measurement APIs and first-error behavior, without touching hardware."""
import os
import subprocess

import pytest


def host_env():
    env = os.environ.copy()
    env.pop("GCC_ROOT", None)
    return env


def test_at110_measurement_shutdown(repo_root, tmp_path):
    power = repo_root / "modules/nsx-power"
    core = repo_root / "modules/nsx-core"
    fixture = repo_root / "tests/fixtures/at110-shutdown"
    command = ["cc", "-std=c11", "-DPART_atomiq110", "-Wall", "-Wextra", "-Werror"]
    for directory in (fixture, power / "includes-api", power / "src",
                      core / "includes-api", core / "includes-internal"):
        command += ["-I", str(directory)]
    binary = tmp_path / "shutdown"
    subprocess.run([*command, str(fixture / "main.c"), str(power / "src/atomiq110/nsx_power_shutdown.c"),
                    "-o", str(binary)], check=True, env=host_env())
    subprocess.run([str(binary)], check=True, timeout=5)


@pytest.mark.parametrize(("compiler", "language"), [("cc", "c"), ("c++", "c++")])
@pytest.mark.parametrize("target", ["PART_atomiq110", "AM_PART_ATOMIQ110", "AM_PART_ATOMIQ11X_API", "AM_PART_APOLLO510"])
def test_measurement_nvm_surface(repo_root, compiler, language, target):
    command = [compiler, "-x", language, "-fsyntax-only", "-Werror", f"-D{target}",
               "-I", str(repo_root / "modules/nsx-power/includes-api"),
               "-I", str(repo_root / "modules/nsx-core/includes-api"), "-"]
    supported = target == "AM_PART_APOLLO510"
    source = '#include "nsx_power.h"\n'
    condition = "ifndef" if supported else "ifdef"
    probe = f'#{condition} NSX_POWER_DRAIN_NVM\n#error unexpected drain macro visibility\n#endif\n'
    subprocess.run(command, input=source + probe, text=True, check=True, env=host_env())
    # Taking the address requires a real declaration even in legacy C compiler modes.
    result = subprocess.run(command, input=source + 'void (*request)(void) = &nsx_power_disable_nvm;\n',
                            capture_output=True, text=True, env=host_env())
    if supported:
        assert result.returncode == 0, result.stderr
    else:
        assert result.returncode != 0 and "nsx_power_disable_nvm" in result.stderr, result.stderr
