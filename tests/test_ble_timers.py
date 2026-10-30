"""Exercise production BLE CCC/expiry routing with real Cordio declarations."""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess

import pytest


@pytest.fixture(scope="module")
def ble_binary(repo_root: Path, tmp_path_factory: pytest.TempPathFactory) -> Path:
    compiler = shutil.which("cc")
    assert compiler, "Host C compiler required"
    fixture = repo_root / "tests/fixtures/ble-timers"
    cordio = repo_root / "modules/nsx-cordio/sdk/third_party/cordio"
    # Match the public Cordio include surface; the fixture replaces only the
    # platform compatibility/HCI boundary, never BLE or Cordio types.
    include_paths = [fixture, repo_root / "modules/nsx-ble/includes-api"]
    include_paths += [cordio / path for path in (
        "wsf/include", "wsf/sources", "wsf/sources/util", "wsf/sources/port/freertos",
        "ble-host/include", "ble-host/sources/stack/cfg", "ble-host/sources/stack/hci",
        "ble-host/sources/stack/dm", "ble-host/sources/stack/l2c",
        "ble-host/sources/stack/att", "ble-host/sources/stack/smp",
        "ble-host/sources/sec/common", "ble-host/sources/hci/ambiq", "ble-profiles/include/app",
        "ble-profiles/sources/apps", "ble-profiles/sources/apps/app",
        "ble-profiles/sources/profiles/include", "ble-profiles/sources/profiles",
        "ble-profiles/sources/profiles/gatt", "ble-profiles/sources/services",
    )]
    output = tmp_path_factory.mktemp("ble-timers") / "ble"
    command = [compiler, "-std=c11", "-g", "-O1", "-DAM_PART_APOLLO5B",
               "-Werror=implicit-function-declaration", "-ffunction-sections", "-fdata-sections",
               "-fsanitize=address,undefined", "-fno-pie", "-no-pie"]
    command += [flag for path in include_paths for flag in ("-I", str(path))]
    command += [str(fixture / "main.c"),
                str(cordio / "ble-host/sources/stack/att/att_uuid.c"),
                "-Wl,--gc-sections", "-o", str(output)]
    env = os.environ.copy()
    env.pop("GCC_ROOT", None)
    result = subprocess.run(command, capture_output=True, text=True, env=env)
    assert result.returncode == 0, result.stdout + result.stderr
    return output


@pytest.mark.parametrize("scenario", [
    "async-enable", "async-expiry", "explicit-send", "sync-success", "sync-failure",
    "sync-null", "mixed-disable", "unknown",
])
def test_ble_notification_timers(ble_binary: Path, scenario: str) -> None:
    result = subprocess.run([str(ble_binary), scenario], capture_output=True,
                            text=True, timeout=5)
    assert result.returncode == 0, result.stdout + result.stderr
