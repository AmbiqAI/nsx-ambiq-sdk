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


def test_only_part_keeps_full_train_for_promotion(repo_root: Path, tmp_path: Path, monkeypatch) -> None:
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
    # promote-only reuses the manifest written by the original full build; it
    # refuses to run without one, so provide it.
    artifacts = tmp_path / "artifacts"
    artifacts.mkdir()
    (artifacts / "manifest.yaml").write_text("sdk:\n  provider: ambiqsuite\n", encoding="utf-8")
    monkeypatch.setattr(helper, "artifact_root", lambda train, version: artifacts)
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
    assert calls["manifest"] == []
    assert calls["promote"] == [(expected_parts, expected_boards)]


def test_promote_only_without_a_manifest_fails_closed(repo_root: Path, tmp_path: Path, monkeypatch) -> None:
    """Synthesizing a manifest here would mint provenance for archives this run
    never built: `write_manifest` hashes whatever bytes are on disk and stamps
    them with the source identity passed on the command line."""
    helper = load_build_ambiqsuite(repo_root)
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
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
    # Artifact root with archives but no manifest.yaml to reuse.
    monkeypatch.setattr(helper, "artifact_root", lambda train, version: tmp_path / "artifacts")
    monkeypatch.setattr(helper, "missing_artifact_libraries", lambda train, version: [])
    monkeypatch.setattr(helper, "built_artifact_toolchains", lambda train, version: ["gcc"])
    monkeypatch.setattr(helper, "write_manifest", lambda *args, **kwargs: wrote.append("wrote"))
    monkeypatch.setattr(helper, "promote_provider_payload", lambda *args, **kwargs: promoted.append("promoted"))

    assert helper.main() == 2
    assert wrote == []
    assert promoted == []


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


def _generated_manifest(entries: list[tuple[str, str]]) -> str:
    """Render the fragment of a generated artifact manifest that
    `scan_manifest_artifacts` reads, in the exact shape `write_manifest` emits."""
    lines = ["parts:"]
    for path, digest in entries:
        lines += ["  - logical_skew: apollo510", "    hal_artifacts:", "      gcc:",
                  f"        path: {path}", f"        sha256: {digest}"]
    return "\n".join(lines) + "\n"


def test_promoted_payload_must_match_the_manifest_promoted_with_it(repo_root: Path, tmp_path: Path) -> None:
    """The check whose absence let v5.2.23 ship archives and a manifest that
    described different bytes."""
    helper = load_build_ambiqsuite(repo_root)
    root = tmp_path / "sdk"
    archive = root / "lib" / "gcc" / "apollo510" / "libam_hal.a"
    archive.parent.mkdir(parents=True)
    archive.write_bytes(b"archive-contents")
    digest = helper.sha256(archive)
    manifest = root / "artifact-manifest.yaml"

    manifest.write_text(_generated_manifest([("gcc/lib/apollo510/libam_hal.a", digest)]), encoding="utf-8")
    assert helper.verify_promoted_manifest_agreement(root) == []

    manifest.write_text(_generated_manifest([("gcc/lib/apollo510/libam_hal.a", "0" * 64)]), encoding="utf-8")
    assert helper.verify_promoted_manifest_agreement(root) == ["lib/gcc/apollo510/libam_hal.a (sha256 mismatch)"]

    archive.unlink()
    assert helper.verify_promoted_manifest_agreement(root) == ["lib/gcc/apollo510/libam_hal.a (missing)"]


def test_promoted_payload_verification_fails_closed_on_degenerate_manifests(
    repo_root: Path, tmp_path: Path
) -> None:
    helper = load_build_ambiqsuite(repo_root)
    root = tmp_path / "sdk"
    root.mkdir()
    manifest = root / "artifact-manifest.yaml"

    assert helper.verify_promoted_manifest_agreement(root)[0].startswith("<missing")

    manifest.write_text("parts: []\n", encoding="utf-8")
    assert helper.verify_promoted_manifest_agreement(root) == [
        "<manifest declares no hal_artifacts/bsp_artifacts entries>"
    ]

    manifest.write_text(_generated_manifest([("gcc/lib/../../../etc/passwd", "0" * 64)]), encoding="utf-8")
    assert "unexpected manifest path shape" in helper.verify_promoted_manifest_agreement(root)[0]

    manifest.write_text("        path: gcc/lib/apollo510/libam_hal.a\n        note: nope\n", encoding="utf-8")
    with pytest.raises(helper.ArtifactManifestMismatch, match="no sha256"):
        helper.verify_promoted_manifest_agreement(root)

    manifest.write_text("        sha256: " + "0" * 64 + "\n", encoding="utf-8")
    with pytest.raises(helper.ArtifactManifestMismatch, match="unpaired"):
        helper.verify_promoted_manifest_agreement(root)
