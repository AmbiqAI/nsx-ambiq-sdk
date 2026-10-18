# Changelog

All notable distribution changes are recorded here.

## 5.2.26 - unreleased

- C++ translation units that include the AmbiqSuite HAL now build with ATfE
  (clang++); they failed with `#error Compiler is unknown` and related errors
  (issue #81, PR #83). Intake patches
  `sdk-intake/patches/stable/001-clang-cxx-compiler-detection.patch` and
  `002-clang-cxx-unnamed-bitfield-qualifiers.patch` change 26 provider
  files, 40 lines: 23 headers and 3 `devices/am_devices_mspi_psram_*.c`
  sources, which no NSX module compiles. C builds, gcc builds and armclang builds select the same
  code as before, and register struct layout is unchanged. Recorded as the
  `clang-cxx-header-compat` ownership exception and known deviation; the
  upstream AmbiqSuite fix is tracked in issue #82.
- `nsx-psram` lists `apollo510dL_evb` and `apollo510L` as compatible (issue
  #76, PR #77). The driver must still be chosen explicitly with
  `NSX_PSRAM_USE_BA_DRIVER`; the rev 2 EVB has no PSRAM fitted.
- CI workflows declare read-only token permissions (PR #80).
- The apollo2, apollo3 and apollo3p GCC linker scripts in `nsx-core` define
  `__HeapBase` / `__HeapLimit`, which the force-linked `_sbrk` needs (issue
  #85, PR #86). Apollo3/Apollo3P GCC and ATfE apps link for the first time;
  they failed already on `v5.2.24`. Verified with `ble_webble` on the Apollo3
  Blue Plus EVB (gcc and ATfE). apollo2 still does not build, for an unrelated
  reason. Known open issue (#87): C++ global constructors do not run on
  apollo2/apollo3/apollo3p. Their `startup_gcc.c` never calls
  `__libc_init_array`, and their linker scripts define no `.init_array`
  section or bounds.
- Bumps the BLE DIS default firmware revision to `5.2.26`.
- No archive changes: every HAL/BSP archive and the artifact manifest are
  byte-identical to `v5.2.25`. All 67 non-atomiq110 archives are
  byte-identical to `v5.2.24`.
- `v5.2.25` was tagged but its GitHub release was not published, so boards
  moving from `5.2.24` also receive the 5.2.25 content below for the first
  time. `release/nsx-ambiq-sdk-5.2.26.yaml` (`consumer_upgrade`) lists the
  changes per starting version.
- Corrects the `nsx-cordio` vendored-revision label. 5.2.25 recorded it as
  `stable-2026.09.01 (3062036c4f)`, but the cordio/uECC sources were never
  re-vendored; they are the `stable-2026.06.18` (`caaf5af860`) tree plus NSX
  edits. The label now says so, and `NSX_VENDORED_VERSION.txt` and the module
  README list the NSX edits. No cordio/uECC source change.
- Records the provider payload's real provenance, unchanged since 5.2.25.
  Apollo headers and archives are `stable-2026.06.18` (`caaf5af8`) content.
  The experimental atomiq110 headers are the `npu-drop-2026.07.09` payload.
  The atomiq110 atfe/acfe archives are recorded as built from `3062036c4f`,
  and its gcc archives come from an unrecorded, older source. The release
  title and the per-train `archive_sets` say the same. The atomiq110 headers
  and archives do not fully match; this is recorded as the open known
  deviation `atomiq110-header-archive-mismatch`.
- CI checks out full git history, so the record's archive and upgrade-path
  checks run there, and they fail rather than skip when history is missing
  under `CI=true`.

## 5.2.25 - 2026-09-04 (tagged, not published)

- Carries the atomiq110 platform work merged after the immutable `v5.2.24`
  tag (PR #46): atomiq110 HAL/BSP artifacts, `nsx-power`/`nsx-core` atomiq110
  backends, and the new `nsx-npu` module (Ethos-U85 glue), plus atomiq110
  STIMER timebase unit tests and an on-FPGA hardware test.
- Re-pins the AmbiqSuite provider snapshot to `stable-2026.09.01`, source
  commit `3062036c4f3e7822d27e6a842f226f423328a25a` (includes the RTL drop 10
  register sync).
- Rebuilds the atomiq110 HAL/BSP archives for all three toolchain trains
  (gcc, atfe/ATfE 22.1.0, acfe/ACfE 6.24 with verified
  `-fshort-wchar -fshort-enums` ABI) from that snapshot. All non-atomiq110
  archives are carried over byte-unchanged from the `5.2.23`/`5.2.24`
  payload.
- Records the atomiq110 INTERNAL-marker sanitation (deletion-only,
  tool-reproducible, no artifact hash changes; issue #52) and the
  `nsx-ethos-u-driver` external-module scope boundary as known deviations.
- `nsx-ethos-u-driver` is not part of this distribution; it is resolved from
  `AmbiqAI/nsx-ethos-u-driver` (pinned at `nsx-ethos-u-driver-v0.1.2`)
  by the neuralspotx registry at workspace-assembly time.

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
