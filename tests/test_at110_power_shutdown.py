"""AT110 measurement APIs and first-error behavior, without touching hardware."""
import os
import subprocess

import pytest

from test_small_m55_memory import compiler_for_test
from tools.nsx_npu_compile_check import compile_flags, read_soc_facts


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
@pytest.mark.parametrize(("target", "supported"), [
    (None, False), ("PART_atomiq110", False), ("AM_PART_ATOMIQ110", False),
    ("AM_PART_ATOMIQ11X_API", False), ("AM_PART_APOLLO330P", False),
    ("AM_PART_APOLLO5A", True), ("AM_PART_APOLLO5B", True), ("AM_PART_APOLLO510", True),
    ("AM_PART_APOLLO510B", True), ("AM_PART_APOLLO510L", True),
])
def test_measurement_nvm_surface(repo_root, compiler, language, target, supported):
    command = [compiler, "-x", language, "-fsyntax-only", "-Werror",
               "-I", str(repo_root / "modules/nsx-power/includes-api"),
               "-I", str(repo_root / "modules/nsx-core/includes-api"), "-"]
    if target:
        command.insert(1, f"-D{target}")
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

@pytest.mark.parametrize("hal_first", [True, False])
@pytest.mark.parametrize("language", ["c", "c++"])
def test_at110_hal_only_header_order(repo_root, hal_first, language):
    gcc = compiler_for_test("gcc")
    # Use the pinned HAL headers with no board or SoC selection macro.
    flags = [flag for flag in compile_flags(repo_root, read_soc_facts(repo_root))
             if not flag.startswith(("-D", "-std="))]
    headers = ["am_mcu_apollo.h", "nsx_power.h"]
    if not hal_first:
        headers.reverse()
    source = "".join(f'#include "{header}"\n' for header in headers)
    result = subprocess.run([str(gcc), *flags, "-I", str(repo_root / "modules/nsx-power/includes-api"),
                             "-x", language, "-E", "-dM", "-"], input=source, text=True,
                            capture_output=True, check=True)
    assert "#define AM_PART_ATOMIQ11X_API " in result.stdout
    assert "#define NSX_POWER_DRAIN_NVM(" not in result.stdout
