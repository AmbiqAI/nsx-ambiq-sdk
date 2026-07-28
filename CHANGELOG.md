# Changelog

All notable distribution changes are recorded here.

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
