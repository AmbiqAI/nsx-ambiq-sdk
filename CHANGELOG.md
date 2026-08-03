# Changelog

All notable distribution changes are recorded here.

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
- Adds `tests/test_artifact_baseline.py`, which verifies the real committed
  payload against the real committed manifest in CI. This is the check that was
  missing when the drift was introduced; it fails on the `5.2.23` tree.
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
