"""AT110 peripheral headers must match the runtime bundled in each HAL archive."""
from __future__ import annotations

import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess

import pytest

from test_cmake_contract import configure_contract_project
from test_small_m55_memory import compiler_for_test


@pytest.mark.parametrize("family,artifact", (("gcc", "gcc"), ("atfe", "atfe"), ("armclang", "acfe")))
def test_at110_peripheral_runtime_link(repo_root: Path, tmp_path: Path, family: str, artifact: str) -> None:
    if family == "armclang":
        found = shutil.which("armclang")
        if not found:
            pytest.skip("ArmClang unavailable")
        compiler = Path(found)
    else:
        compiler = compiler_for_test(family)
    source = tmp_path / "peripheral.c"
    source.write_text('''#include "am_mcu_apollo.h"
#include <stddef.h>
_Static_assert(sizeof(am_hal_periph_lock_t) == 8, "bundled mutex storage");
_Static_assert(offsetof(am_hal_periph_lock_t, name) == 4, "bundled mutex name");
_Static_assert(sizeof(am_hal_periph_token_t) == 16, "bundled semaphore storage");
_Static_assert(offsetof(am_hal_periph_token_t, name) == 12, "bundled semaphore name");
_Static_assert(AM_HAL_XSPIMC_GLOBAL_RAM_SIZE == 712, "controller and token capacity");
void *make_lock(const osMutexAttr_t *attributes) { return osMutexNew(attributes); }
''')
    result = configure_contract_project(repo_root, tmp_path, "atomiq110_fpga_turbo", family,
        prelude=("set(CMAKE_EXPORT_COMPILE_COMMANDS ON)",
                 f"set(NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN {artifact})"),
        post_board_include=(f'add_library(peripheral OBJECT "{source.as_posix()}")',
                            "target_link_libraries(peripheral PRIVATE nsx::ambiq_hal)"))
    assert result.returncode == 0, result.stdout
    build = tmp_path / family / "atomiq110_fpga_turbo/build"
    entry, = [item for item in json.loads((build / "compile_commands.json").read_text())
              if item["file"] == str(source)]
    command = shlex.split(entry["command"])
    command[0] = str(compiler)
    obj = tmp_path / "peripheral.o"
    command[command.index("-o") + 1] = str(obj)
    result = subprocess.run(command, cwd=entry["directory"], text=True, capture_output=True)
    assert result.returncode == 0, result.stdout + result.stderr
    gcc = compiler_for_test("gcc")
    archive = repo_root / f"modules/nsx-ambiqsuite/sdk/lib/{artifact}/atomiq110/libam_hal.a"
    linked = tmp_path / "peripheral-linked.o"
    result = subprocess.run([str(gcc.parent / "arm-none-eabi-ld"), "-r", "--fatal-warnings",
        "-z", "noexecstack", "-u", "sOPS", str(obj), str(archive), "-o", str(linked)],
        text=True, capture_output=True)
    assert result.returncode == 0, result.stdout + result.stderr
    symbols = subprocess.check_output([str(gcc.parent / "arm-none-eabi-nm"), str(linked)], text=True)
    for name in ("osDelay", "osKernelGetTickFreq", "osMutexNew", "osMutexAcquire", "osMutexRelease",
                 "osMutexDelete", "osSemaphoreNew", "osSemaphoreAcquire", "osSemaphoreRelease",
                 "osSemaphoreDelete"):
        assert re.search(rf"^\S+ T {name}$", symbols, re.MULTILINE), name
    assert not re.search(r"^\s+U os[A-Z]", symbols, re.MULTILINE), symbols
