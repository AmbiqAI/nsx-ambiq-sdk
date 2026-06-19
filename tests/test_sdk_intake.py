from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path

import pytest


def load_build_ambiqsuite(repo_root: Path):
    module_path = repo_root / "sdk-intake" / "build_ambiqsuite.py"
    spec = importlib.util.spec_from_file_location("test_build_ambiqsuite", module_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _apollo2_train(helper):
    """The unified `stable` train narrowed to its single legacy part (apollo2 /
    apollo2_evb, gcc+atfe). Gives these unit tests a small, fully-enumerable
    provider without standing up all 8 parts / 14 boards."""
    full = helper.TRAINS["stable"]
    return helper.replace(
        full,
        parts=tuple(p for p in full.parts if p.name == "apollo2"),
        boards=tuple(b for b in full.boards if b.part == "apollo2"),
    )


def test_write_manifest_uses_portable_tool_names(repo_root: Path, tmp_path: Path, monkeypatch) -> None:
    helper = load_build_ambiqsuite(repo_root)
    manifest_root = tmp_path / "artifacts"
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()

    monkeypatch.setattr(helper, "artifact_root", lambda train, version: manifest_root)
    monkeypatch.setattr(helper, "find_upstream_revision", lambda sdk_root: None)

    profiles = {
        "gcc": helper.ToolchainProfile(
            name="gcc",
            config="bin",
            compiler="arm-none-eabi-gcc",
            archive_tool="arm-none-eabi-ar",
            make_vars=(),
        ),
        "atfe": helper.ToolchainProfile(
            name="atfe",
            config="bin-atfe",
            compiler="/toolchains/ATfE/bin/clang",
            archive_tool="/toolchains/ATfE/bin/llvm-ar",
            make_vars=(),
        ),
    }

    helper.write_manifest(
        _apollo2_train(helper),
        "stable-2026.06.17",
        sdk_root,
        profiles,
        source_kind="git_ref",
        source_ref="stable",
        source_commit="deadbeef",
        debug_symbols=False,
    )

    text = (manifest_root / "manifest.yaml").read_text(encoding="utf-8")
    assert "/toolchains/ATfE" not in text
    assert "compiler: clang" in text
    # archive_tool basenames containing '-' are YAML-quoted by the writer.
    assert "archive_tool: 'llvm-ar'" in text
    assert "arm-none-eabi-gcc" in text


def test_only_part_keeps_full_train_for_manifest_and_promotion(repo_root: Path, tmp_path: Path, monkeypatch) -> None:
    helper = load_build_ambiqsuite(repo_root)
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    calls: dict[str, list[tuple[list[str], list[str]]]] = {"manifest": [], "promote": []}

    monkeypatch.setattr(
        helper,
        "parse_args",
        lambda: argparse.Namespace(
            train="stable",
            only_part=["apollo510"],
            version="stable-2026.06.17",
            toolchain=[],
            promote=False,
            promote_only=True,
            fpu=None,
            debug_symbols=False,
            verbose=False,
        ),
    )
    monkeypatch.setattr(helper, "resolve_source_root", lambda args: (sdk_root, "git_ref", "stable", "deadbeef"))
    monkeypatch.setattr(helper, "selected_toolchains", lambda train, values: ["gcc"])
    monkeypatch.setattr(helper, "optional_toolchain_profile", lambda args, name: None)
    # Empty artifact root (no manifest.yaml) so promote-only takes the write path.
    monkeypatch.setattr(helper, "artifact_root", lambda train, version: tmp_path / "artifacts")
    # The full train's artifact set is treated as already complete on disk so the
    # promotion guard passes without depending on locally-built (gitignored) trees.
    monkeypatch.setattr(helper, "missing_artifact_libraries", lambda train, version: [])
    monkeypatch.setattr(helper, "built_artifact_toolchains", lambda train, version: ["gcc"])
    monkeypatch.setattr(
        helper,
        "write_manifest",
        lambda train, version, sdk_root, profiles, **kwargs: calls["manifest"].append(
            ([part.name for part in train.parts], [board.name for board in train.boards])
        ),
    )
    monkeypatch.setattr(
        helper,
        "promote_provider_payload",
        lambda train, version, sdk_root: calls["promote"].append(
            ([part.name for part in train.parts], [board.name for board in train.boards])
        ),
    )

    assert helper.main() == 0

    full_train = helper.TRAINS["stable"]
    expected_parts = [part.name for part in full_train.parts]
    expected_boards = [board.name for board in full_train.boards]
    assert calls["manifest"] == [(expected_parts, expected_boards)]
    assert calls["promote"] == [(expected_parts, expected_boards)]


def test_promote_only_reuses_existing_manifest(repo_root: Path, tmp_path: Path, monkeypatch) -> None:
    helper = load_build_ambiqsuite(repo_root)
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    artifacts = tmp_path / "artifacts"
    artifacts.mkdir()
    # A manifest from the original full build already exists on disk.
    (artifacts / "manifest.yaml").write_text("sdk:\n  provider: ambiqsuite\n", encoding="utf-8")
    wrote: list[str] = []
    promoted: list[str] = []

    monkeypatch.setattr(
        helper,
        "parse_args",
        lambda: argparse.Namespace(
            train="stable",
            only_part=[],
            version="stable-2026.06.17",
            toolchain=[],
            promote=False,
            promote_only=True,
            fpu=None,
            debug_symbols=False,
            verbose=False,
        ),
    )
    monkeypatch.setattr(helper, "resolve_source_root", lambda args: (sdk_root, "git_ref", "stable", "deadbeef"))
    monkeypatch.setattr(helper, "selected_toolchains", lambda train, values: ["gcc"])
    monkeypatch.setattr(helper, "optional_toolchain_profile", lambda args, name: None)
    monkeypatch.setattr(helper, "artifact_root", lambda train, version: artifacts)
    monkeypatch.setattr(helper, "missing_artifact_libraries", lambda train, version: [])
    monkeypatch.setattr(helper, "built_artifact_toolchains", lambda train, version: ["gcc"])
    monkeypatch.setattr(helper, "write_manifest", lambda *a, **k: wrote.append("called"))
    monkeypatch.setattr(helper, "promote_provider_payload", lambda train, version, sdk_root: promoted.append(version))

    assert helper.main() == 0
    # The existing manifest is reused, never regenerated with empty profiles.
    assert wrote == []
    assert promoted == ["stable-2026.06.17"]


def test_build_promote_rejects_unprofiled_built_toolchain(repo_root: Path, tmp_path: Path, monkeypatch) -> None:
    helper = load_build_ambiqsuite(repo_root)
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    promoted: list[str] = []

    monkeypatch.setattr(
        helper,
        "parse_args",
        lambda: argparse.Namespace(
            train="stable",
            only_part=[],
            version="stable-2026.06.17",
            toolchain=[],
            promote=True,
            promote_only=False,
            fpu=None,
            debug_symbols=False,
            verbose=False,
        ),
    )
    monkeypatch.setattr(helper, "resolve_source_root", lambda args: (sdk_root, "git_ref", "stable", "deadbeef"))
    # Build nothing this run; atfe artifacts exist on disk from a prior build but
    # ATFE_ROOT is not configured, so its profile resolves to None.
    monkeypatch.setattr(helper, "selected_toolchains", lambda train, values: [])
    monkeypatch.setattr(helper, "optional_toolchain_profile", lambda args, name: None)
    monkeypatch.setattr(helper, "write_manifest", lambda *a, **k: None)
    monkeypatch.setattr(helper, "missing_artifact_libraries", lambda train, version: [])
    monkeypatch.setattr(helper, "built_artifact_toolchains", lambda train, version: ["atfe"])
    monkeypatch.setattr(helper, "promote_provider_payload", lambda train, version, sdk_root: promoted.append(version))

    # The build+promote run refuses rather than publish a manifest that marks atfe
    # built but omits its compiler/archive metadata.
    assert helper.main() == 2
    assert promoted == []


def _make_artifact(root: Path, toolchain: str, *parts: str) -> None:
    for relative in parts:
        path = root / toolchain / "lib" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"archive")


def test_promote_artifact_libraries_rejects_incomplete_toolchain(repo_root: Path, tmp_path: Path, monkeypatch) -> None:
    helper = load_build_ambiqsuite(repo_root)
    train = _apollo2_train(helper)  # part apollo2, board apollo2_evb, toolchains gcc + atfe
    version = "test"
    artifacts = tmp_path / "artifacts"
    provider = tmp_path / "provider"
    provider.mkdir()
    (provider / "keep.txt").write_text("keep", encoding="utf-8")
    monkeypatch.setattr(helper, "artifact_root", lambda t, v: artifacts)
    monkeypatch.setattr(helper, "provider_sdk_root", lambda t: provider)

    # gcc is complete; atfe is materialized but missing the board BSP archive.
    _make_artifact(artifacts, "gcc", "apollo2/libam_hal.a", "apollo2/apollo2_evb/libam_bsp.a")
    _make_artifact(artifacts, "atfe", "apollo2/libam_hal.a")

    assert helper.missing_artifact_libraries(train, version) == ["atfe/lib/apollo2/apollo2_evb/libam_bsp.a"]
    with pytest.raises(FileNotFoundError):
        helper.promote_artifact_libraries(train, version)
    # The existing payload must survive a rejected (incomplete) promotion.
    assert (provider / "keep.txt").is_file()
    assert not (provider / "lib").exists()


def test_promote_artifact_libraries_promotes_complete_set(repo_root: Path, tmp_path: Path, monkeypatch) -> None:
    helper = load_build_ambiqsuite(repo_root)
    train = _apollo2_train(helper)
    version = "test"
    artifacts = tmp_path / "artifacts"
    provider = tmp_path / "provider"
    monkeypatch.setattr(helper, "artifact_root", lambda t, v: artifacts)
    monkeypatch.setattr(helper, "provider_sdk_root", lambda t: provider)

    for toolchain in ("gcc", "atfe"):
        _make_artifact(artifacts, toolchain, "apollo2/libam_hal.a", "apollo2/apollo2_evb/libam_bsp.a")

    assert helper.missing_artifact_libraries(train, version) == []
    helper.promote_artifact_libraries(train, version)

    # Promoted layout drops the upstream lib/ segment: lib/<toolchain>/<part>[/<board>].
    for toolchain in ("gcc", "atfe"):
        assert (provider / "lib" / toolchain / "apollo2" / "libam_hal.a").is_file()
        assert (provider / "lib" / toolchain / "apollo2" / "apollo2_evb" / "libam_bsp.a").is_file()


def test_default_extract_dir_derives_from_zip_stem(repo_root: Path) -> None:
    helper = load_build_ambiqsuite(repo_root)
    extract_dir = helper.default_extract_dir(Path("/drops/AmbiqSuite_R5.2.0.zip"))
    assert extract_dir.name == "AmbiqSuite_R5.2.0"
    assert "None" not in extract_dir.name
