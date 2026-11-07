"""Execute production EM9305 RX state machines across mocked HAL/WSF boundaries."""
from pathlib import Path
import shutil
import subprocess

import pytest


@pytest.fixture(scope="module")
def em9305_binary(repo_root: Path, tmp_path_factory: pytest.TempPathFactory) -> Path:
    compiler = shutil.which("cc")
    assert compiler, "Host C compiler required"
    fixture = repo_root / "tests/fixtures/em9305-rx"
    cordio = repo_root / "modules/nsx-cordio/sdk/third_party/cordio"
    device = repo_root / "modules/nsx-ambiqsuite/sdk/devices"
    build = tmp_path_factory.mktemp("em9305-rx")
    source = (device / "am_devices_em9305.c").read_text()
    # Compile the exact vendor preamble, RDY helpers and block_read function.
    # Unrelated firmware programming/power routines are not host-emulated.
    start = source.index("uint32_t\nam_devices_em9305_block_read(")
    end = source.index("\n//*****************************************************************************", start)
    prefix = source[:source.index("void\nam_devices_em9305_config_pins(")]
    sliced = build / "device_rx.c"
    sliced.write_text(prefix + "\nvoid SPI_write_byte(uint8_t);\nuint8_t SPI_read_byte(void);\n" + source[start:end])
    paths = [fixture, device] + [cordio / p for p in (
        "wsf/include", "wsf/sources", "wsf/sources/util", "wsf/sources/port/freertos", "ble-host/include",
        "ble-host/sources/stack/hci", "ble-host/sources/stack/cfg",
        "ble-host/sources/hci/ambiq", "ble-host/sources/hci/ambiq/em9305",
    )]
    output = build / "em9305"
    command = [compiler, "-std=c11", "-g", "-O1", "-DAM_PART_APOLLO510",
               "-Werror=implicit-function-declaration", "-ffunction-sections", "-fdata-sections",
               "-fsanitize=address,undefined", "-fno-pie", "-no-pie"]
    command += [flag for path in paths for flag in ("-I", str(path))]
    command += [str(fixture / "main.c"), str(sliced), "-Wl,--gc-sections",
                "-Wl,--wrap=am_devices_em9305_block_read", "-o", str(output)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    return output


@pytest.mark.parametrize("scenario", [
    "rx256", "rx257", "rx258", "rx259", "irq", "wrap", "empty", "empty-tx",
    "empty-budget", "partial", "not-ready", "transfer-error", "length-error",
    "device-empty", "device-prior-empty", "device-full", "device-full-wrap",
    "device-full-no-irq", "device-command-error", "device-data-error",
    "device-not-ready", "device-259", "device-stale-counter", "integrated-full", "integrated-empty",
])
def test_em9305_receive_contract(em9305_binary: Path, scenario: str) -> None:
    result = subprocess.run([str(em9305_binary), scenario], capture_output=True,
                            text=True, timeout=5)
    assert result.returncode == 0, result.stdout + result.stderr
