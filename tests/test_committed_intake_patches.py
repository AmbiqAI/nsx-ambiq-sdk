"""The committed provider payload must already carry every intake patch.

Patches under ``sdk-intake/patches/<train>/`` are applied by
``intake_workflow.py stage``, but ``build_ambiqsuite.py --promote`` writes the
payload without running that hook. A payload regenerated that way would
silently drop a patch while every other test still passes, so this checks the
committed tree directly: each patch must load with its sidecar and must
reverse-apply cleanly against ``modules/nsx-ambiqsuite/sdk``.
"""

from __future__ import annotations

import importlib.util
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[1]
PATCHES_ROOT = REPO_ROOT / "sdk-intake" / "patches"
PAYLOAD_RELPATH = "modules/nsx-ambiqsuite/sdk"

PATCHES = sorted(PATCHES_ROOT.glob("*/*.patch"))


def _load_intake_workflow():
    module_path = REPO_ROOT / "sdk-intake" / "intake_workflow.py"
    spec = importlib.util.spec_from_file_location("committed_patches_intake_workflow", module_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize("train_dir", sorted({patch.parent for patch in PATCHES}), ids=lambda p: p.name)
def test_patch_queue_loads_with_sidecars(train_dir: Path) -> None:
    queue = _load_intake_workflow().load_patch_queue(train_dir)
    assert len(queue) == len(list(train_dir.glob("*.patch")))


@pytest.mark.skipif(shutil.which("git") is None, reason="git is required to check patches")
@pytest.mark.parametrize("patch", PATCHES, ids=lambda p: f"{p.parent.name}/{p.name}")
def test_committed_payload_carries_patch(patch: Path) -> None:
    result = subprocess.run(
        [
            "git",
            "apply",
            "--reverse",
            "--check",
            f"--directory={PAYLOAD_RELPATH}",
            str(patch),
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, (
        f"{patch.relative_to(REPO_ROOT)} is not applied to {PAYLOAD_RELPATH}; a regeneration "
        f"dropped it. Re-stage with intake_workflow.py or re-apply the patch.\n{result.stderr}"
    )
