# Qualification: nsx-ambiq-sdk 5.2.25

Date: 2026-09-03
Status: prepared for release. Not tagged or published; publication requires
explicit approval per `docs/release-process.md`.

## Decision

Version `5.2.25` is qualified as a content update of `5.2.24`. It carries the
atomiq110 platform work that landed on `main` after the immutable `v5.2.24`
tag (see `release/qualification-5.2.25-prep.md`), re-pins the AmbiqSuite
provider snapshot to the commit the promoted atomiq110 archives were actually
built from, and rebuilds the atomiq110 HAL/BSP archives for all three
toolchain trains from that snapshot.

`5.2.24` remains published and immutable. Its tag, release, and commit
`a9f4ec25a162f6f3700623feb691423bb5a51132` are not moved, deleted, or
rewritten.

The provider input is now: snapshot `stable-2026.09.01`, ref `stable`, commit
`3062036c4f3e7822d27e6a842f226f423328a25a` (includes the RTL drop 10 register
sync, ambiqsuite `78bb7682`/`ebc4158a`). As in prior releases, this is not
blanket board-level hardware qualification and not vendor SDK release
qualification. SWS AmbiqSuite remains the source of truth for HAL/BSP
implementation and silicon qualification.

## What Changed From 5.2.24

| Change | Path |
| --- | --- |
| atomiq110 platform support: HAL/BSP artifacts, `nsx-power`/`nsx-core` atomiq110 backends, new `nsx-npu` module (Ethos-U85 glue) — PR #46 | `modules/nsx-ambiqsuite/sdk/`, `modules/nsx-npu/` |
| AmbiqSuite snapshot re-pinned to `stable-2026.09.01` / `3062036c4f` | `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml`, `modules/nsx-ambiqsuite/nsx-module.yaml` |
| atomiq110 gcc HAL/BSP archives rebuilt from `3062036c4f` (commit `d5a1a932`) | `modules/nsx-ambiqsuite/sdk/lib/gcc/atomiq110/` |
| atomiq110 atfe (ATfE 22.1.0) and acfe (ACfE 6.24, `-fshort-wchar -fshort-enums` verified) archives rebuilt from `3062036c4f` (commit `21383316`) | `modules/nsx-ambiqsuite/sdk/lib/{atfe,acfe}/atomiq110/` |
| atomiq110 INTERNAL-marker sanitation (deletion-only, tool-reproducible, no artifact hash changes) — issue #52 | `sdk-intake/internal_markers.py`, `release/source-ownership.yaml` |
| atomiq110 STIMER timebase unit tests and on-FPGA hardware test | `modules/nsx-npu/tests/` |

All non-atomiq110 HAL/BSP archives are carried over byte-unchanged from the
`5.2.23`/`5.2.24` payload, whose object code was built from
`caaf5af86087881647f56c70646c748d40c86e23`; those archives were not rebuilt
against `3062036c4f` (recorded in `payload_provenance.carried_over_archives_note`).

## Scope

Qualification scope is inherited from `release/qualification-5.2.24.md`
unchanged. atomiq110 remains **experimental** (FPGA bring-up,
`atomiq110_fpga_turbo`) and stays outside the qualified archive scope.

`nsx-npu` requires `nsx-ethos-u-driver`, which is deliberately **not** part of
this distribution; it is resolved from `AmbiqAI/nsx-ethos-u-driver`
(pinned at `nsx-ethos-u-driver-v0.1.2`) by the neuralspotx registry at
workspace-assembly time.

## Evidence

- `release/nsx-ambiq-sdk-5.2.25.yaml` (machine-verified by
  `tests/test_release_metadata.py`)
- `release/qualification-5.2.25-prep.md` (queued-content record)
- `tests/test_artifact_baseline.py` (committed payload vs committed manifest)
- `docs/acfe-artifact-manifest-forensics.md` (acfe provenance history)
