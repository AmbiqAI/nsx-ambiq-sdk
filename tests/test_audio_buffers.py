"""Exercise unmodified PDM backends with bounded 32-bit mock DMA addresses."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

import pytest

from test_cmake_contract import configure_contract_project
from test_small_m55_memory import compiler_for_test


@pytest.fixture(scope="module", params=("apollo3", "apollo5"))
def audio_binary(request: pytest.FixtureRequest, repo_root: Path, tmp_path_factory: pytest.TempPathFactory) -> Path:
    compiler = shutil.which("cc")
    assert compiler, "A host C compiler is required for audio safety tests"
    dest = tmp_path_factory.mktemp(request.param)
    fixture = repo_root / "tests/fixtures/audio"
    source = repo_root / f"modules/nsx-audio/src/{request.param}/nsx_pdm_{request.param}.c"
    binary = dest / "audio"
    flags = ["-DTEST_APOLLO3"] if request.param == "apollo3" else []
    env = os.environ.copy()
    env.pop("GCC_ROOT", None)
    result = subprocess.run([compiler, "-std=c11", "-g", "-O1", "-fno-pie", "-no-pie",
        "-fsanitize=address,undefined", *flags, f"-I{fixture}",
        f"-I{repo_root / 'modules/nsx-audio/includes-api'}", str(source), str(fixture / "main.c"),
        "-o", str(binary)], env=env, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return binary


@pytest.mark.parametrize("scenario", ("valid", "stereo", "24bit", "overflow", "channel-overflow",
    "zero", "channels", "width", "unknown-width", "high-address", "dma-short", "pcm-short", "dma-align", "pcm-align",
    "address-wrap", "instance", "count-too-large", "count-limit"))
def test_audio_buffer_bounds(audio_binary: Path, scenario: str) -> None:
    result = subprocess.run([str(audio_binary), scenario], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize("board", ("apollo3_evb", "apollo3p_evb", "apollo510_evb", "apollo510b_evb", "apollo330mP_evb", "apollo510dL_evb"))
@pytest.mark.parametrize("family", ("gcc", "atfe", "armclang"))
def test_audio_real_headers(repo_root: Path, tmp_path: Path, board: str, family: str) -> None:
    if family == "armclang":
        found = shutil.which("armclang")
        if not found:
            pytest.skip("ArmClang unavailable")
        compiler = Path(found)
    else:
        compiler = compiler_for_test(family)
    result = configure_contract_project(repo_root, tmp_path, board, family,
        ("nsx-core", "nsx-audio"), prelude=("set(CMAKE_EXPORT_COMPILE_COMMANDS ON)",))
    assert result.returncode == 0, result.stdout
    build = tmp_path / family / board / "build"
    entries = json.loads((build / "compile_commands.json").read_text())
    selected = [entry for entry in entries if "/nsx_pdm_" in entry["file"]]
    assert len(selected) == 1
    command = shlex.split(selected[0]["command"])
    command[0] = str(compiler)
    command[command.index("-o") + 1] = str(tmp_path / "audio.o")
    result = subprocess.run(command, cwd=selected[0]["directory"], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
