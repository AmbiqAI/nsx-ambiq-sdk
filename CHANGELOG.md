# Changelog

All notable distribution changes are recorded here.

## 5.2.25 - 2026-08-27

- Adds the **experimental** `atomiq110` platform (Cortex-M55 + Arm Ethos-U85 on
  the `atomiq110_fpga_turbo` FPGA board): SoC and board descriptors, promoted
  HAL/BSP archives for all three toolchain trains, startup and linker scripts,
  and the `nsx-power`/`nsx-core` atomiq110 backends.
- Adds the `nsx-npu` module — Ethos-U85 power-domain sequencing, IRQ wiring,
  and performance-mode selection on top of the pristine upstream driver.
- Bounds the Ethos-U85 inference wait. `nsx-npu` supplies the STIMER timebase
  hooks and `cmake/socs/atomiq110.cmake` seeds
  `NSX_ETHOSU_INFERENCE_TIMEOUT_MS=5000`, so a wedged NPU returns
  `ETHOSU_JOB_RESULT_TIMEOUT` and takes the soft-reset recovery path instead of
  hanging the caller forever. A timebase that fails to arm is logged and
  readable through `nsx_npu_timebase_status()`.
- `atomiq110` and `atomiq110_fpga_turbo` remain **outside the qualified archive
  scope**: FPGA bring-up only, no silicon, and an uncharacterised FPGA clock
  tree. Every qualified SoC and board is unchanged from `5.2.24`.
- **No promoted Apollo archive bytes change.** The only archive-level change is
  additive: six new atomiq110 archives. Every qualified target still comes from
  AmbiqSuite snapshot `stable-2026.06.18`, source commit
  `caaf5af86087881647f56c70646c748d40c86e23`. The atomiq110 archives come from
  the separate `npu-drop-2026.07.09` payload, recorded per artifact in the
  artifact manifest and as a `known_deviations` entry.
- `nsx-ethos-u-driver` is **deliberately not part of this distribution**. It is
  registry-resolved from `AmbiqAI/nsx-ethos-u-driver` at
  `nsx-ethos-u-driver-v0.1.2` by neuralspotx at workspace-assembly time.
  Consumers must link `nsx::ethos_u_driver` PUBLIC.
- Records a one-time, deletion-only INTERNAL-marker sanitation of the atomiq110
  headers (issue #52): 988 lines removed across 23 files, zero added or
  modified, no artifact hash changed. `modules/nsx-ambiqsuite/sdk` is
  `direct_edit: forbidden`, so it is carried as a scoped ownership exception in
  `release/source-ownership.yaml`; future payloads are sanitized while staged.
- Dedupes the atomiq110 XTAL/VCOMP shutdown sequence (#53), the power-profile
  printers (#54), and the Apollo5-class platform backend including DCU unlock.
- Bumps the BLE Device Information Service default firmware revision to
  `5.2.25`.
- `5.2.23` and `5.2.24` remain published and immutable; their qualification
  reports are left exactly as released.

## 5.2.24 - 2026-08-03

- Corrects the promoted artifact manifest, which recorded superseded `sha256`
  values for 22 of the 23 `acfe` (armclang) HAL/BSP archives shipped in
  `5.2.23`. The archives themselves were always correct; only their recorded
  provenance was stale.
- **No promoted archive bytes change.** The binary payload of `5.2.24` is
  identical to `5.2.23`, still built from AmbiqSuite snapshot
  `stable-2026.06.18`, source ref `stable`, source commit
  `caaf5af86087881647f56c70646c748d40c86e23`.
- Records `acfe` ABI provenance (`-fshort-wchar -fshort-enums`) in the artifact
  manifest, and emits ABI-affecting flags from
  `sdk-intake/build_ambiqsuite.py` so future intakes keep recording them.
- Hardens the promotion path so the drift cannot recur:
  `promote_provider_payload` now verifies that the manifest it promotes
  describes the archives it promoted, and `--promote-only` fails closed instead
  of synthesizing a manifest for archives it did not build.
- Adds `tests/test_artifact_baseline.py`, which verifies the real committed
  payload against the real committed manifest in CI, pins the published archive
  inventory, and rejects symlinked or undeclared archives. This is the check
  that was missing when the drift was introduced; it fails on the `5.2.23` tree.
- Bumps the BLE Device Information Service default firmware revision to
  `5.2.24` and pins it to the distribution version in tests; it had silently
  stayed at the previous version through a release.
- Documents the full investigation, per-artifact hashes, root-cause commit,
  ownership, and impact in `docs/acfe-artifact-manifest-forensics.md`.
- `5.2.23` remains published and immutable. Its tag, release, and commit
  `2eba24ad776096784764cbe91c8176b434dd3bdf` are unchanged; this correction is
  published as a new distribution version per `docs/versioning.md`.

## 5.2.23 - 2026-07-28

- Establishes the first immutable tagged golden baseline for `nsx-ambiq-sdk`.
- Pins the promoted provider payload to AmbiqSuite snapshot
  `stable-2026.06.18`, source ref `stable`, source commit
  `caaf5af86087881647f56c70646c748d40c86e23`. Repository evidence does not
  identify this snapshot as an official numbered AmbiqSuite release.
- Includes the qualified NSX adapter scope for staged Apollo2, Apollo3,
  Apollo4, Apollo330, and Apollo510 SoCs and boards, including the coherent
  `nsx-psram` 1.0 API.
- Aligns every manifest under `modules/` to distribution version `5.2.23`
  while preserving independent public API versions such as
  `NSX_PSRAM_V1_0_0`.
- Excludes descriptor-only `apollo5b` / `apollo5b_evb` and experimental
  `atomiq110` / `atomiq110_fpga_turbo` from the qualified release scope.
- Records software, descriptor, artifact, representative link-smoke, and
  limited feature-level hardware evidence in
  `release/qualification-5.2.23.md`; no release-wide hardware qualification
  is claimed.
