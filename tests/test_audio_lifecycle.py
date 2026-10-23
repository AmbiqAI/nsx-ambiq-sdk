"""Fault-inject production PDM lifecycle operations and require safe state."""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess

import pytest


@pytest.fixture(scope="module")
def lifecycle_binary(request: pytest.FixtureRequest, repo_root: Path, tmp_path_factory: pytest.TempPathFactory) -> tuple[Path, str]:
    compiler = shutil.which("cc")
    assert compiler
    dest = tmp_path_factory.mktemp(request.param + "-lifecycle")
    fixture = repo_root / "tests/fixtures/audio"
    source = repo_root / f"modules/nsx-audio/src/{request.param}/nsx_pdm_{request.param}.c"
    binary = dest / "lifecycle"
    flags = ["-DTEST_APOLLO3"] if request.param == "apollo3" else ["-DAM_PART_APOLLO5B"]
    env = os.environ.copy()
    env.pop("GCC_ROOT", None)
    result = subprocess.run([compiler, "-std=c11", "-g", "-O1", "-DTEST_LIFECYCLE", "-fno-pie", "-no-pie",
        "-fsanitize=address,undefined", *flags, f"-I{fixture}", f"-I{repo_root / 'modules/nsx-audio/includes-api'}",
        str(source), str(fixture / "lifecycle.c"), "-o", str(binary)], env=env, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return binary, request.param


CASES = [("init", step) for step in ("clock", "initialize", "power-on", "configure", "flush", "threshold", "irq-clear", "irq-enable")]
CASES += [("start", step) for step in ("enable", "dma-start")]
CASES += [("stop", step) for step in ("irq-disable", "irq-clear", "dma-stop", "disable", "power-off", "deinit")]
CASES += [("isr", step) for step in ("status", "irq-clear", "service")]
CASES += [("ping-pong", "none"), ("bad-buffer", "none"), ("threshold-error", "none")]
CASES += [("ownership", "none"), ("callback-stop", "none"), ("callback-restart", "none")]
CASES += [("init-cleanup", step) for step in ("power-on/power-off", "configure/power-off", "configure/deinit")]
CASES += [("clock-policy", fault) for fault in ("init-hp", "start-hp", "init-query", "start-query", "hfrc-hp")]
CASES += [("hardware-error", bit + suffix) for bit in ("derr", "undfl", "ovf") for suffix in ("", "+dcmp")]


def applicable(backend: str, phase: str, fault: str) -> bool:
    if backend != "apollo3":
        return True
    if fault in ("clock", "dma-stop") or phase == "isr" and fault != "service":
        return False
    return phase not in ("ping-pong", "bad-buffer", "threshold-error", "clock-policy")


# Unsupported operations are outside this backend's matrix, not skipped tests.
# Any skip in a selected, applicable case must still fail intake qualification.
@pytest.mark.parametrize("lifecycle_binary,phase,fault", [
    (backend, phase, fault) for backend in ("apollo3", "apollo5")
    for phase, fault in CASES if applicable(backend, phase, fault)
], indirect=["lifecycle_binary"], scope="module")
def test_audio_lifecycle_failure(lifecycle_binary: tuple[Path, str], phase: str, fault: str) -> None:
    binary, backend = lifecycle_binary
    result = subprocess.run([str(binary), phase, fault], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
