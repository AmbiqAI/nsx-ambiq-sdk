"""Exercise the shipped EM9305 framing/writer with full-length HAL RX writes."""
import hashlib
from pathlib import Path
import shutil
import subprocess

import pytest


@pytest.fixture(scope="module")
def device_tx_binary(repo_root: Path, tmp_path_factory: pytest.TempPathFactory) -> Path:
    compiler = shutil.which("cc")
    assert compiler, "Host C compiler required"
    fixture = repo_root / "tests/fixtures/em9305-rx"
    device = repo_root / "modules/nsx-ambiqsuite/sdk/devices"
    cordio = repo_root / "modules/nsx-cordio/sdk/third_party/cordio"
    build = tmp_path_factory.mktemp("em9305-device-tx")
    source = (device / "am_devices_em9305.c").read_text()
    # Retain actual constants, globals, RDY/CS helpers and TX routines. Omit
    # unrelated firmware programming and peripheral initialization routines.
    sliced = source[:source.index("void\nam_devices_em9305_config_pins(")]
    for signature in ("uint8_t\nam_devices_em9305_tx_starts(",
                      "void\nam_devices_em9305_tx_ends(",
                      "uint32_t am_devices_em9305_block_write("):
        start = source.index(signature)
        end = source.index("\n//", source.index("\n}", start) + 2)
        sliced += "\n" + source[start:end]
    (build / "device_tx_source.c").write_text(sliced)
    output = build / "device-tx"
    paths = [fixture, build, device, cordio / "wsf/include", cordio / "wsf/sources", cordio / "wsf/sources/util",
             cordio / "wsf/sources/port/freertos"]
    command = [compiler, "-std=c11", "-g", "-O1", "-DAM_PART_APOLLO510",
               "-Werror=implicit-function-declaration", "-ffunction-sections", "-fdata-sections",
               "-fsanitize=address,undefined", "-fno-pie", "-no-pie"]
    command += [flag for path in paths for flag in ("-I", str(path))]
    command += [str(fixture / "device_tx.c"), "-Wl,--gc-sections", "-o", str(output)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    return output


@pytest.mark.parametrize("length,credit,mode", [
    (16, 16, "ok"), (17, 17, "ok"), (255, 255, "ok"),
    (259, 16, "ok"), (259, 17, "ok"), (259, 255, "ok"),
    (260, 255, "ok"), (0, 255, "ok"), (17, 0, "ok"),
    (17, 17, "enable-error"), (17, 17, "header-error"),
    (17, 17, "data-error"), (17, 17, "not-ready"), (259, 17, "bitbang"),
])
def test_em9305_device_transmit(device_tx_binary: Path, length: int, credit: int, mode: str) -> None:
    result = subprocess.run([str(device_tx_binary), str(length), str(credit), mode],
                            capture_output=True, text=True, timeout=5)
    assert result.returncode == 0, result.stdout + result.stderr


def test_em9305_device_patch_bytes(repo_root: Path, tmp_path: Path) -> None:
    """Reconstruct the complete pre-fix device file, not just its patch hunk."""
    relative = Path("devices/am_devices_em9305.c")
    actual = (repo_root / "modules/nsx-ambiqsuite/sdk" / relative).read_bytes()
    target = tmp_path / relative
    target.parent.mkdir()
    target.write_bytes(actual)
    patch = repo_root / "sdk-intake/patches/stable/004-em9305-tx-receive-scratch.patch"
    for reverse in (True, False):
        command = ["git", "apply"] + (["--reverse"] if reverse else []) + [str(patch)]
        result = subprocess.run(command, cwd=tmp_path, capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result.stderr
        if reverse:
            # Complete generated device file at ef84a93, before issue195's patch.
            assert hashlib.sha256(target.read_bytes()).hexdigest() == "58b783297295841f4420e4f22e65b95c2d8b3eb8fb8367e314b9ae2ee3b02944"
    assert target.read_bytes() == actual
