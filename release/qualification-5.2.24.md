# Qualification: nsx-ambiq-sdk 5.2.24

Date: 2026-08-03

## Decision

Version `5.2.24` is qualified as a provenance correction of `5.2.23`. It
republishes the identical promoted AmbiqSuite payload with an artifact manifest
that describes it correctly.

`5.2.23` remains published and immutable. Its tag, release, and commit
`2eba24ad776096784764cbe91c8176b434dd3bdf` are not moved, deleted, or rewritten.

The provider input is unchanged: snapshot `stable-2026.06.18`, ref `stable`,
commit `caaf5af86087881647f56c70646c748d40c86e23`. As in `5.2.23`, this is not
blanket board-level hardware qualification and not vendor SDK release
qualification. SWS AmbiqSuite remains the source of truth for HAL/BSP
implementation and silicon qualification.

## What Changed From 5.2.23

**No promoted archive bytes changed.** Every HAL/BSP archive in `5.2.24` is
byte-identical to the archive published in `5.2.23`.

| Change | Path |
| --- | --- |
| 22 `acfe` `sha256` records corrected to the archives actually shipped | `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml` |
| `acfe` ABI provenance recorded (`abi_cflags`) | `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml` |
| ABI-affecting flags now emitted into generated manifests | `sdk-intake/build_ambiqsuite.py` |
| Committed-payload hash verification added to CI | `tests/test_artifact_baseline.py` |
| Forensic record of the defect | `docs/acfe-artifact-manifest-forensics.md` |

The defect, its root cause in `ddb88640e61660edc65ebc956b65dcbd6804d2e6`, the
full per-artifact hash table, ownership, and impact are recorded in
[`docs/acfe-artifact-manifest-forensics.md`](../docs/acfe-artifact-manifest-forensics.md).

## Software And Build Evidence

| Layer | Status | Evidence and scope |
| --- | --- | --- |
| Repository CI | Passed | `uv sync --group ci`; pre-commit; intake-helper `py_compile`; full pytest. |
| Static/contracts | Passed | Manifest, repository-shape, toolchain, intake, public API, and CMake contract tests. |
| Descriptor configure | Passed | CMake contract tests validate staged SoC/board descriptors and required artifact paths. |
| Promoted artifacts | **Verified in CI** | `tests/test_artifact_baseline.py` recomputes sha256 for all 73 declared HAL/BSP archives against the committed manifest and rejects undeclared archives. Equivalent to `intake_workflow.py verify-baseline --train stable`. |
| ACfE ABI attributes | Verified | `fromelf --decode_build_attributes` (ArmClang 6.24Rel19) reports `wchar_t = 2` and smallest-container enums for all 23 committed `acfe` archives. |
| GCC link smoke | Inherited from 5.2.23 | GCC 15.2.1 minimal HAL/BSP/SoC links for `apollo330mP_evb`, `apollo510_evb`, `apollo510b_evb`, `apollo510dL_evb`. Archives are byte-identical, so the evidence carries over unchanged. |
| ArmClang link smoke | Inherited from 5.2.23 | ArmClang 6.24.0 minimal HAL/BSP/SoC links for the same four representative boards, against these exact `acfe` archives. |
| ATfE link smoke | Inherited from 5.2.23 | ATfE 22.1.0 minimal HAL/BSP/SoC links for the same four representative boards. |

Link-smoke evidence is inherited rather than re-run because the archives, SoC
descriptors, board descriptors, and NSX wrappers are unchanged from `5.2.23`;
only manifest metadata, a test, and documentation differ. The qualified
descriptor scope is the `qualification.scope` list in
`release/nsx-ambiq-sdk-5.2.24.yaml` and is unchanged from `5.2.23`.

## Hardware Evidence

Unchanged from `5.2.23` and inherited without extension, because no shipped
binary or descriptor changed:

- Bounded newlib `_sbrk` selection: hardware-validated on `apollo330mP_evb` and
  `apollo510_evb`.
- `nsx-psram` BA-driver selection, capacity, timing scan, synchronous transfers,
  and XIP behavior: hardware-validated on `apollo510_evb` and `apollo510b_evb`.
- `nsx-psram` XIP/DMA read-write: hardware-validated on
  `apollo4p_evb_disp_shield_rev2`.

These records do not qualify every module or peripheral on those boards. There is
no complete board-farm matrix. No broader hardware coverage is inferred, and no
new hardware evidence is claimed for `5.2.24`.

## Exclusions

Unchanged from `5.2.23`:

- `apollo5b` and `apollo5b_evb` are descriptor-only. Matching promoted HAL/BSP
  artifacts are absent, so they are not configure/link qualified.
- `atomiq110` and `atomiq110_fpga_turbo` remain experimental and are excluded
  from the official qualified scope even though descriptors and artifacts are
  present.
- Optional-module support remains limited to each module's declared
  compatibility and evidence.

## Known Deviations

- The BLE Device Information Service default firmware-revision string still
  reads `5.2.23`. It is an overridable runtime default rather than release or
  provenance metadata, and one of its two copies is third-party Cordio source
  declared `direct_edit: restricted`. Recorded under `known_deviations` in
  `release/nsx-ambiq-sdk-5.2.24.yaml` and tracked as follow-up work.

## Reproduction Boundary

Verification of this baseline requires no proprietary source: the committed
archives are hash-verified against the committed manifest in CI. Re-deriving the
archives from AmbiqSuite source at
`caaf5af86087881647f56c70646c748d40c86e23` requires read-only access to the
proprietary tree plus the licensed toolchains, on controlled internal
infrastructure. That remains a manual/internal step outside this repository, as
described in [`docs/intake-hardening.md`](../docs/intake-hardening.md).
