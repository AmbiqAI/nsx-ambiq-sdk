# Qualification: nsx-ambiq-sdk 5.2.26

Date: 2026-10-18
Status: prepared for release. Not tagged or published; publication requires
explicit approval per `docs/release-process.md`.

## Decision

Version `5.2.26` is qualified as a content update of `5.2.25`. Its main change
is the clang C++ header compatibility fix for the AmbiqSuite provider headers
(issue #81, PR #83). It also carries the `nsx-psram` compatibility entries for
`apollo510dL_evb` and `apollo510L` (PR #77) and CI workflow token permissions
(PR #80).

`v5.2.25` is tagged (`aefce2ca858795e783c76726ebe7d14d9d4bde7c`) but its GitHub
release was never published. `v5.2.24`
(`a9f4ec25a162f6f3700623feb691423bb5a51132`) is the last published release.
Both tags are immutable and are not moved, deleted, or rewritten.

neuralSPOT-X moves every board to `5.2.26`: `atomiq110_fpga_turbo` from
`5.2.25`, every other board from `5.2.24`. So this report, and the
`consumer_upgrade` section of `release/nsx-ambiq-sdk-5.2.26.yaml`, describe the
changes from both versions.

The provider identity is unchanged from `5.2.25`: snapshot
`stable-2026.09.01`, ref `stable`, commit
`3062036c4f3e7822d27e6a842f226f423328a25a`. This is not blanket board-level
hardware qualification and not vendor SDK release qualification. SWS
AmbiqSuite remains the source of truth for HAL/BSP implementation and silicon
qualification.

## What Changed From 5.2.25

No archive changes: every HAL/BSP archive (73) and
`modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml` are byte-identical to
`v5.2.25`.

| Change | Path |
| --- | --- |
| clang C++ header compatibility: intake patches 001 and 002, 26 headers, 40 lines (issue #81, PR #83) | `sdk-intake/patches/stable/`, `modules/nsx-ambiqsuite/sdk/{mcu,devices}/` |
| `clang-cxx-header-compat` ownership exception and guard test | `release/source-ownership.yaml`, `tests/test_committed_intake_patches.py` |
| `nsx-psram` lists `apollo510dL_evb` and `apollo510L` as compatible; the driver is still chosen explicitly (issue #76, PR #77) | `modules/nsx-psram/` |
| CI workflow token permissions (PR #80) | `.github/workflows/` |
| BLE DIS default firmware revision `5.2.26` | `modules/nsx-ble/src/ns_ble.c`, `modules/nsx-cordio/.../svc_dis.c` |

Applying patches 001 and 002 to the `v5.2.25` provider tree with
`git apply --directory=modules/nsx-ambiqsuite/sdk` gives the `5.2.26` tree
byte-for-byte.

The header patch changes which branch is selected only where compilation
used to fail: C++ with clang (and C89/C++98 modes). C with gcc or clang, C++
with gcc, and armclang (matched first by `__ARMCC_VERSION`) select the same code
as before. Unnamed padding bit-fields cannot be accessed, so dropping their
qualifier changes neither layout nor behaviour.

## What Changed From 5.2.24 (non-atomiq110 boards)

Boards other than `atomiq110_fpga_turbo` move from `5.2.24`, so they also
receive the `5.2.25` content for the first time.

**Prebuilt archives: unchanged.** The 67 non-atomiq110 HAL/BSP archives (gcc,
atfe and acfe) are byte-identical to `v5.2.24`, and their `sha256` records in
the artifact manifest are unchanged. `git diff --stat v5.2.24 origin/main --
'modules/nsx-ambiqsuite/sdk/lib/**'` lists only the six atomiq110 archives.
`tests/test_release_metadata.py::test_consumer_upgrade_archive_claims_match_git_history`
checks this against git.

**Provider headers:** the only non-atomiq110 change is the clang C++ patch set
(23 files outside the atomiq110 directories). None of these files changed
between `v5.2.24` and `v5.2.25`.

| Change | Affects non-atomiq110 builds | Path |
| --- | --- | --- |
| clang C++ header patches (above) | Yes: headers for every part | `sdk-intake/patches/stable/` |
| Provider identity re-pinned to `stable-2026.09.01` / `3062036c4f` | Metadata only: non-atomiq110 headers and archives stay the `caaf5af8` content | `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml`, `modules/nsx-ambiqsuite/nsx-module.yaml` |
| `nsx-core` Apollo5-class platform backend moved into a shared `.inc` (PR #57) | Yes: Apollo5-class parts and apollo330P. The PR reports byte-identical objects for `apollo510_evb` (gcc 15.2.rel1) | `modules/nsx-core/src/` |
| `nsx-core` `nsx_mem.h` / `nsx_cache.c` | No: new `AM_PART_ATOMIQ110` branches only | `modules/nsx-core/` |
| `nsx-power` shared power-profile printer (PR #56) | Yes: format unchanged, CSV dump no longer waits 1 ms per row on Apollo5-class parts, installed `src/` gains two files; public API unchanged | `modules/nsx-power/` |
| `nsx-ambiq-hal` PMU init | No: atomiq110-only path under `AM_PART_ATOMIQ110` | `modules/nsx-ambiq-hal/src/nsx_ambiq_pmu.c` |
| `nsx-psram` compatibility (PR #77) | Yes: `apollo510dL_evb` / `apollo510L` | `modules/nsx-psram/` |
| `nsx-ble` / `nsx-cordio` DIS firmware revision `5.2.24` -> `5.2.26` | Yes: advertised string | `modules/nsx-ble/`, `modules/nsx-cordio/` |
| `nsx-cordio` vendored-revision label moved to `stable-2026.09.01 (3062036c4f)` | Label only: sources not re-vendored (known deviation) | `modules/nsx-cordio/` |
| atomiq110 platform, `nsx-npu`, atomiq110 power/core backends | No: atomiq110 targets only | `modules/nsx-npu/`, `*/src/atomiq110/`, `cmake/socs/atomiq110.cmake` |
| Intake/CI tooling (INTERNAL-marker sanitizer, nsx-npu cross-compile job, patch guard test, token permissions) | No: not part of a consumer build | `sdk-intake/`, `tests/`, `.github/` |

## Known Deviations

Recorded in `release/nsx-ambiq-sdk-5.2.26.yaml` (`known_deviations`):

- `clang-cxx-header-compat` (new): intake patches 001 and 002 are applied
  directly to the committed provider headers, because the payload cannot be
  re-staged from one AmbiqSuite commit (issue #82). Upstream fix in progress on
  AmbiqSuite branch `bugfix/clang-cxx-header-compat` (`48bd9bd576`); the
  AmbiqSuite pull request is not yet open. Retire both patches once a
  regenerated payload carries the fix.
- `mixed-snapshot-provider-payload` (now explicit, unchanged since 5.2.25):
  only atomiq110 content comes from `3062036c4f`; everything else is the
  `caaf5af8` content plus the patches.
- `nsx-cordio-vendored-revision-label` (new finding, present since 5.2.25):
  the label reads `3062036c4f`, but the vendored cordio/uecc sources match
  AmbiqSuite `caaf5af860` (plus 7 NSX-modified files that predate `v5.2.24`).
  Nine upstream cordio files that changed between the two commits are not
  carried. Checked read-only against a local AmbiqSuite clone.
- `atomiq110-internal-marker-sanitation` and `nsx-ethos-u-driver-external`:
  carried from 5.2.25.

## Software And Build Evidence

| Layer | Status | Evidence and scope |
| --- | --- | --- |
| Repository CI | Passed locally; confirm on the release commit | `uv sync --group ci`; `pre-commit run --all-files`; `py_compile` of both intake helpers; full pytest. CI must be green on the exact commit that is tagged. |
| Release metadata | Passed | `tests/test_release_metadata.py`, including the upgrade-path archive check against `v5.2.24` and `v5.2.25` history, and the check that every provider ownership exception and intake patch is a declared deviation. |
| Promoted artifacts | Passed | `intake_workflow.py verify-baseline --train stable`: 73 archives match the manifest. |
| Ownership boundary | Passed | `intake_workflow.py verify-ownership --train stable --patches-dir <abs>/sdk-intake/patches/stable`. |
| INTERNAL markers | Passed | `intake_workflow.py verify-internal-markers --train stable`. |
| Committed intake patches | Passed | `tests/test_committed_intake_patches.py`: both patches reverse-apply against the committed payload. |
| nsx-npu cross-compile (CI `arm-compile` job) | Passed locally | `tests/test_npu_cross_compile.py` with `NSX_REQUIRE_ARM_GCC=1`, `NSX_REQUIRE_ETHOS_U_DRIVER=1`, `nsx-ethos-u-driver-v0.1.2`, arm-none-eabi-gcc 15.2.1 (CI pins 14.3.rel1). |
| GCC link smoke | Passed | `tools/nsx_link_smoke.py --toolchain-family gcc`, arm-none-eabi-gcc 15.2.1: `apollo330mP_evb`, `apollo510_evb`, `apollo510b_evb`, `apollo510dL_evb`. The same C smoke extended to all 16 non-descriptor-only boards also passes. |
| ATfE link smoke | Passed | `tools/nsx_link_smoke.py --toolchain-family atfe`, ATfE 22.1.0: same four boards; extended C smoke passes on all 16 boards. |
| ATfE C++ link smoke | **Passed (new)** | One C++17 translation unit including `am_mcu_apollo.h` and `am_bsp.h` and calling `am_bsp_low_power_init()`, linked against `nsx::board`, `nsx::ambiq_bsp` and `nsx::soc_hal`, with `-fno-exceptions`: 16/16 boards pass on 5.2.26 and 0/16 on `7a4ac9f` (before #83), which fail with `Compiler is unknown`, the literal-suffix error, and the qualified anonymous bit-field error. With exceptions enabled, 11/16 pass; apollo2/apollo3/apollo3p boards then fail at link on `_exit` from the C library's `abort` (no syscall stubs in the minimal smoke), after the C++ file compiles. |
| GCC C++ link smoke | Passed, no regression | Same C++ unit with arm-none-eabi-g++ 15.2.1 and `-fno-exceptions`: 16/16 on 5.2.26 and on `7a4ac9f`. |
| ArmClang link smoke | **Not run** | ACfE is not installed in the qualification environment. The acfe archives are byte-identical to 5.2.25, and the header patches do not change the armclang branch. Last ArmClang evidence: the 5.2.25 / 5.2.23 records. A missing licensed toolchain is a recorded gap, not an implied pass. |

## Hardware Evidence

No new hardware evidence is claimed for `5.2.26`. The records inherited from
`5.2.23`/`5.2.24` stand unchanged for the non-atomiq110 archives, which are
byte-identical. PR #77 records that `apollo510dL_evb` rev 2 firmware built with
either PSRAM driver reports `psram_init_failed`, because that board has no
PSRAM fitted. The `nsx-psram` compatibility entry lets the module resolve on
that board; it is not a hardware qualification of PSRAM on it.

## Exclusions

Unchanged:

- `apollo5b` and `apollo5b_evb` are descriptor-only.
- `atomiq110` and `atomiq110_fpga_turbo` remain experimental and outside the
  qualified scope.
- Optional-module support remains limited to each module's declared
  compatibility and evidence.

## Reproduction Boundary

Verification of this payload requires no proprietary source: the committed
archives are hash-verified against the committed manifest in CI, and the
committed headers are checked to carry both intake patches. Re-deriving the
payload from AmbiqSuite source is not possible from one commit today (issue
#82) and remains a manual/internal step, as described in
[`docs/intake-hardening.md`](../docs/intake-hardening.md).
