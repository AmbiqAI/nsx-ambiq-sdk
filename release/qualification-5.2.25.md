# Qualification: nsx-ambiq-sdk 5.2.25

Date: 2026-08-27
Status: prepared for release. Not tagged or published; publication requires
explicit approval per `docs/release-process.md`.

**Tag prerequisite.** This record is prepared against `origin/main` plus the
issue #64 timebase hardening (PR #68). `v5.2.25` must not be created until that
work has merged: the tag is what every AT110 build resolves through the
neuralspotx registry, and shipping a bounded inference wait that can silently
fail to arm — or abort a healthy inference — would defeat the purpose of
shipping it at all. Confirm before tagging that the release commit contains it.

## Decision

Version `5.2.25` is qualified as a **forward content release** of `5.2.24`. It
adds the experimental `atomiq110` (Cortex-M55 + Arm Ethos-U85) platform, the
`nsx-npu` module, and NSX-side cleanups. It is not a correction: nothing
`5.2.24` published is withdrawn or restated.

`5.2.23` and `5.2.24` remain published and immutable. Their tags, releases, and
commits — `2eba24ad776096784764cbe91c8176b434dd3bdf` and
`a9f4ec25a162f6f3700623feb691423bb5a51132` — are not moved, deleted, or
rewritten, and `release/qualification-5.2.23.md` and
`release/qualification-5.2.24.md` are left exactly as released.

The provider input for every **qualified** SoC and board is unchanged: snapshot
`stable-2026.06.18`, ref `stable`, commit
`caaf5af86087881647f56c70646c748d40c86e23`. As in `5.2.23` and `5.2.24`, this
is not blanket board-level hardware qualification and not vendor SDK release
qualification. SWS AmbiqSuite remains the source of truth for HAL/BSP
implementation and silicon qualification.

## atomiq110 Is Experimental And Outside Qualified Scope

`atomiq110` and `atomiq110_fpga_turbo` are **excluded from the qualified
archive scope of `5.2.25`**, exactly as they were in `5.2.24`. The exclusion is
recorded in `exclusions.experimental` in
[`release/nsx-ambiq-sdk-5.2.25.yaml`](nsx-ambiq-sdk-5.2.25.yaml) and enforced
by `tests/test_release_metadata.py`.

What is present: SoC and board descriptors, promoted HAL/BSP archives for all
three toolchain trains, the `nsx-power`/`nsx-core` atomiq110 backends, and the
`nsx-npu` Ethos-U85 module. All of it configures and builds.

Why it is still excluded:

- The only realization is an FPGA bring-up board. **No silicon exists.**
- The FPGA clock tree is not characterised. `am_hal_clkgen.h`'s `ATOMIQ11X_FPGA`
  block carries silicon frequencies (100/250/500 MHz) under a literal
  `// TODO: check actual frequencies on FPGA`, while `am_mcu_apollo.h` and
  `cmake/socs/facts/atomiq110.cmake` record the turbo core at 25 MHz. Until
  that is resolved by the register-map owner, any timing-derived claim on this
  part is provisional — including the 5000 ms Ethos-U inference deadline seeded
  in `cmake/socs/atomiq110.cmake`.
- Its archives come from a different upstream payload than the qualified ones
  (see the next section).
- No link-smoke or artifact qualification claim is made for it, and none is
  inherited.

Hardware work on the FPGA has been performed by the platform owner (hello-world
through NPU inference and profiling), but that is bring-up evidence recorded in
pull requests, not qualification evidence, and it is deliberately **not**
promoted into this record. `atomiq110` leaves experimental status no earlier
than silicon qualification.

## Payload Provenance: Two Upstream Identities

`5.2.25` carries **two** upstream payload identities, and the distinction
matters for exactly one reason: every *qualified* target is on the declared
snapshot.

| Targets | Upstream payload | Qualified |
| --- | --- | --- |
| All Apollo SoCs and boards | `stable-2026.06.18` / `caaf5af8` | Yes |
| `atomiq110`, `atomiq110_fpga_turbo` | `npu-drop-2026.07.09` (regenerated in `960624ee`) | No — experimental |

No promoted Apollo archive byte changed. The only archive-level change relative
to `5.2.24` is additive: six new atomiq110 archives (one HAL and one BSP per
train). The divergence is recorded per artifact in
`modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml` and as the
`atomiq110-payload-identity` entry under `known_deviations` in the release
manifest, so it is stated rather than left to be inferred.

## Recorded Divergence: atomiq110 INTERNAL-Marker Sanitation

The atomiq110 headers diverge from the `npu-drop-2026.07.09` payload by a
one-time, **deletion-only** sanitation of vendor `#### INTERNAL ####`
engineering content (issue #52, commit `835d80b0`):

| Property | Value |
| --- | --- |
| Files changed | 23 (all under `modules/nsx-ambiqsuite/sdk/mcu/atomiq110`) |
| Lines removed | 988 |
| Lines added or modified | **0** |
| Prebuilt archive hashes changed | **None** |
| Mechanism | `sdk-intake/internal_markers.py` (tool-reproducible, not hand-edited) |

`npu-drop-2026.07.09` is a raw engineering drop: upstream's release tooling
strips these fences and the dead content between them before publishing, and
that step had not been run. Only marker lines of fully-removed blocks, block
bodies with no live preprocessor tokens, `#if 0 … #endif` regions inside a
fence, and blank lines at a deletion seam were deleted. Any block still holding
a live declaration kept its body and its fence verbatim, so **nothing visible
to the compiler that built the prebuilt archives changed** — which is why no
artifact hash moved. Re-running the scrubber over the pre-scrub tree reproduces
the committed tree byte-for-byte, and re-running it over the committed tree is
a no-op.

`modules/nsx-ambiqsuite/sdk` is `direct_edit: forbidden`. This is therefore
recorded as a scoped, reviewed **ownership exception**
(`atomiq110-internal-marker-sanitation` on the `generated-ambiqsuite-provider`
entry in [`release/source-ownership.yaml`](source-ownership.yaml)), not treated
as an ordinary edit, and it is not a precedent for editing the provider tree in
place. Future payloads are sanitized while staged: `stage` runs
`assert_no_new_internal_markers` against
`sdk-intake/internal-marker-baseline.yaml`, so no comparable post-hoc
divergence should recur. Issue #59 tracks the remaining INTERNAL content in the
atomiq110 board BSP headers, deferred to the next payload intake.

## External Dependency: nsx-ethos-u-driver Is Not In This Distribution

`nsx-npu` requires `nsx-ethos-u-driver`, which is **deliberately not part of
this distribution**. It is not vendored, not promoted, and not qualified here.

| | |
| --- | --- |
| Repository | `AmbiqAI/nsx-ethos-u-driver` |
| Pinned release | **`nsx-ethos-u-driver-v0.1.2`** |
| Resolved by | neuralspotx at workspace-assembly time, from `src/neuralspotx/data/registry.lock.yaml` |
| Shipped here | No |

It holds the pristine upstream Arm `ethos-u-core-driver` plus the NSX
wait-semaphore, cache, and remap glue; `nsx-npu` contributes only the
atomiq110 power-domain, IRQ, and performance-mode glue and the timebase
overrides on top of it. Because the driver's weak-symbol timebase overrides
ship through `INTERFACE_SOURCES` rather than as archive members, **consumers
must link `nsx::ethos_u_driver` PUBLIC**; PRIVATE linkage silently strips them
and restores upstream's unbounded inference wait.

This scope boundary supersedes the working prep note
`release/qualification-5.2.25-prep.md`, removed at release-prep time, which
recorded the first driver tag `v0.1.1`. The pinned release is `v0.1.2`.

## What Changed From 5.2.24

| Change | Path |
| --- | --- |
| atomiq110 SoC/board descriptors, HAL/BSP archives, startup and linker scripts | `cmake/socs/atomiq110.cmake`, `boards/atomiq110_fpga_turbo/`, `modules/nsx-ambiqsuite/sdk/lib/*/atomiq110/`, `modules/nsx-core/src/atomiq110/` |
| New `nsx-npu` module: Ethos-U85 power/IRQ/performance-mode glue | `modules/nsx-npu/` |
| Bounded Ethos-U inference wait: STIMER timebase hooks and the 5000 ms deadline seed | `modules/nsx-npu/src/atomiq110/nsx_npu_timebase.c`, `cmake/socs/atomiq110.cmake` |
| INTERNAL-marker sanitizer and the one-time atomiq110 header scrub (issue #52) | `sdk-intake/internal_markers.py`, `sdk-intake/internal-marker-baseline.yaml`, `modules/nsx-ambiqsuite/sdk/mcu/atomiq110/` |
| atomiq110 `nsx-power` XTAL/VCOMP shutdown dedupe (issue #53, partial) | `modules/nsx-power/src/atomiq110/nsx_power.c` |
| Single shared power-profile printer, per-arch tables (issue #54) | `modules/nsx-power/src/` |
| Apollo5-class platform backend dedupe, incl. DCU unlock | `modules/nsx-core/src/` |
| BLE DIS default firmware revision bumped to the distribution version | `modules/nsx-ble/src/ns_ble.c`, `modules/nsx-cordio/sdk/third_party/cordio/ble-profiles/sources/services/svc_dis.c` |

## Software And Build Evidence

| Layer | Status | Evidence and scope |
| --- | --- | --- |
| Repository CI | Passed locally; confirm on the release commit | `uv sync --group ci`; pre-commit; intake-helper `py_compile`; full pytest. Must be green on the exact commit that is tagged. |
| Static/contracts | Passed | Manifest, repository-shape, toolchain, intake, public API, and CMake contract tests, including the atomiq110 descriptor and `nsx-npu` configure contracts. |
| Descriptor configure | Passed | CMake contract tests validate staged SoC/board descriptors and required artifact paths, and assert that `nsx-npu` fails fast without an `nsx::ethos_u_driver` provider. |
| Promoted artifacts | **Verified in CI** | `tests/test_artifact_baseline.py` recomputes sha256 for every declared HAL/BSP archive against the committed manifest and rejects undeclared or symlinked archives. |
| INTERNAL-marker ratchet | **Verified in CI** | `tests/test_internal_markers.py` re-runs the scrubber and asserts no new markers against `sdk-intake/internal-marker-baseline.yaml`. |
| Release provenance | **Verified in CI** | `tests/test_release_metadata.py` cross-checks this record and the release manifest against the artifact manifest, the provider manifest, the ownership inventory, and git history (`archives_introduced_in`). |
| ACfE ABI attributes | Inherited from 5.2.24 | The `acfe` archives are byte-identical, so the `fromelf --decode_build_attributes` evidence carries over unchanged. |
| GCC link smoke | Inherited from 5.2.24 | Minimal HAL/BSP/SoC links for `apollo330mP_evb`, `apollo510_evb`, `apollo510b_evb`, `apollo510dL_evb`. Archives are byte-identical, so the evidence carries over. |
| ArmClang link smoke | Inherited from 5.2.24 | Same four representative boards, against these exact `acfe` archives. |
| ATfE link smoke | Inherited from 5.2.24 | Same four representative boards. |
| atomiq110 link smoke | **Not run — gap** | `atomiq110_fpga_turbo` is not in `tools/nsx_link_smoke.py`'s staged board list. Recorded as a gap, not an implied pass; it is outside qualified scope. |

Link-smoke evidence is inherited rather than re-run because every qualified
board's archives, SoC descriptors, board descriptors, and NSX wrappers are
unchanged from `5.2.24`. The qualified descriptor scope is the
`qualification.scope` list in
[`release/nsx-ambiq-sdk-5.2.25.yaml`](nsx-ambiq-sdk-5.2.25.yaml) and is
unchanged from `5.2.24`.

## Hardware Evidence

Unchanged from `5.2.24` and inherited without extension, because no shipped
binary or descriptor for a qualified target changed:

- Bounded newlib `_sbrk` selection: hardware-validated on `apollo330mP_evb` and
  `apollo510_evb`.
- `nsx-psram` BA-driver selection, capacity, timing scan, synchronous transfers,
  and XIP behavior: hardware-validated on `apollo510_evb` and `apollo510b_evb`.
- `nsx-psram` XIP/DMA read-write: hardware-validated on
  `apollo4p_evb_disp_shield_rev2`.

These records do not qualify every module or peripheral on those boards. There
is no complete board-farm matrix. No broader hardware coverage is inferred, and
**no new hardware evidence is claimed for `5.2.25`** — in particular, no
atomiq110 FPGA result is promoted into this record.

## Exclusions

- `apollo5b` and `apollo5b_evb` are descriptor-only. Matching promoted HAL/BSP
  artifacts are absent, so they are not configure/link qualified. Unchanged
  from `5.2.24`.
- `atomiq110` and `atomiq110_fpga_turbo` are experimental and excluded from the
  qualified scope, as described above.
- `nsx-ethos-u-driver` is outside this distribution entirely and is neither
  shipped nor qualified here.
- Optional-module support remains limited to each module's declared
  compatibility and evidence.

## Reproduction Boundary

Verification of this baseline requires no proprietary source: the committed
archives are hash-verified against the committed manifest in CI, and the
INTERNAL-marker scrub is reproducible from the committed tooling and baseline.
Re-deriving the Apollo archives from AmbiqSuite source at
`caaf5af86087881647f56c70646c748d40c86e23`, or the atomiq110 archives from the
`npu-drop-2026.07.09` payload, requires read-only access to the proprietary
tree plus the licensed toolchains, on controlled internal infrastructure. That
remains a manual/internal step outside this repository, as described in
[`docs/intake-hardening.md`](../docs/intake-hardening.md).
