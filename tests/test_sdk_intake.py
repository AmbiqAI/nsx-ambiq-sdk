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
        helper.TRAINS["r2"],
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
            train="r5",
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

    full_train = helper.TRAINS["r5"]
    expected_parts = [part.name for part in full_train.parts]
    expected_boards = [board.name for board in full_train.boards]
    assert calls["manifest"] == [(expected_parts, expected_boards)]
    assert calls["promote"] == [(expected_parts, expected_boards)]


def _make_artifact(root: Path, toolchain: str, *parts: str) -> None:
    for relative in parts:
        path = root / toolchain / "lib" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"archive")


def test_promote_artifact_libraries_rejects_incomplete_toolchain(repo_root: Path, tmp_path: Path, monkeypatch) -> None:
    helper = load_build_ambiqsuite(repo_root)
    train = helper.TRAINS["r2"]  # part apollo2, board apollo2_evb, toolchains gcc + atfe
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
    train = helper.TRAINS["r2"]
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
