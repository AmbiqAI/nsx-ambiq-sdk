"""Exercise the linked CTIMER backend with the released Apollo3 and 3 Plus headers."""

import subprocess

import pytest


@pytest.mark.parametrize("soc", ["apollo3", "apollo3p"])
def test_ctimer_backend(repo_root, tmp_path, soc):
    modules = repo_root / "modules"
    sdk = modules / "nsx-ambiqsuite/sdk"
    includes = [
        repo_root / "tests/fixtures/ap3-buck",
        sdk / "mcu" / soc,
        sdk / "CMSIS/AmbiqMicro/Include",
        modules / "nsx-power/src",
        modules / "nsx-power/includes-api",
    ]
    executable = tmp_path / "ap3-buck"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Werror",
        *[f"-I{path}" for path in includes],
        str(repo_root / "tests/fixtures/ap3-buck/main.c"), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
