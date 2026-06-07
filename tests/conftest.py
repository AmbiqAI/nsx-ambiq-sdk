from __future__ import annotations

from pathlib import Path

import pytest
import yaml


REPO_ROOT = Path(__file__).resolve().parents[1]
MODULES_DIR = REPO_ROOT / "modules"
BOARDS_DIR = REPO_ROOT / "boards"


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return REPO_ROOT


@pytest.fixture(scope="session")
def module_dirs() -> list[Path]:
    return sorted(path for path in MODULES_DIR.iterdir() if path.is_dir())


@pytest.fixture(scope="session")
def board_dirs() -> list[Path]:
    return sorted(path for path in BOARDS_DIR.iterdir() if path.is_dir())


@pytest.fixture(scope="session")
def manifests(module_dirs: list[Path], board_dirs: list[Path]) -> dict[Path, dict]:
    paths = [*(path / "nsx-module.yaml" for path in module_dirs), *(path / "nsx-module.yaml" for path in board_dirs)]
    loaded = {}
    for path in paths:
        with path.open("r", encoding="utf-8") as handle:
            loaded[path] = yaml.safe_load(handle)
    return loaded
