# Forensics: ACfE Archive / Artifact-Manifest Mismatch In `v5.2.23`

Status: closed by distribution version `5.2.24`.
Investigated: 2026-08-03. Scope: `modules/nsx-ambiqsuite/sdk`.

`v5.2.23` shipped **correct** `acfe` (armclang) HAL/BSP archives with a **stale**
artifact manifest. 22 of the 23 `acfe` entries in
`modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml` recorded the sha256 of
archives that had already been replaced. `gcc` and `atfe` were never affected.

The published `v5.2.23` tag, release, and commit
`2eba24ad776096784764cbe91c8176b434dd3bdf` are immutable and were not touched.
The correction is published as a new distribution version, per
[`versioning.md`](versioning.md).

## Summary

| | |
| --- | --- |
| Defect | Artifact manifest records superseded sha256 values for 22 `acfe` archives |
| Introduced by | `ddb88640e61660edc65ebc956b65dcbd6804d2e6` (PR #22, merged 2026-06-26) |
| First shipped in | `v5.2.23` / `2eba24ad776096784764cbe91c8176b434dd3bdf` (2026-07-28) |
| Detected by | `sdk-intake/intake_workflow.py verify-baseline`, added by PR #49 (`1c6d1512670ae61943d08adeab3eb5591f1689ec`) |
| Functional impact | None. The shipped archives are the correct, ABI-fixed, link-smoke-validated builds |
| Integrity impact | High. The manifest is the payload's provenance record and it was wrong; `verify-baseline` fails against `v5.2.23`, which is indistinguishable from tampering |
| Archive bytes changed by the fix | None |
| Fixed in | `5.2.24` (manifest correction only) |

## Established Facts

Every number below was produced twice: once with the repository's own
`verify-baseline` verifier and once with an independent script that imports no
repository code and recomputes sha256 with `hashlib` directly. Both agree.

### 1. The mismatch is intrinsic to the tag, not a later `main` regression

| Tree | Entries | `gcc` | `atfe` | `acfe` |
| --- | --- | --- | --- | --- |
| `v5.2.23` (`2eba24ad776096784764cbe91c8176b434dd3bdf`) | 73 | 25 match | 25 match | **22 mismatch**, 1 match |
| `origin/main` (`1c6d1512670ae61943d08adeab3eb5591f1689ec`) | 73 | 25 match | 25 match | **22 mismatch**, 1 match |
| Published `v5.2.23` source tarball | 73 | 25 match | 25 match | **22 mismatch**, 1 match |

`git diff 2eba24ad..origin/main -- modules/nsx-ambiqsuite/sdk` is empty: the only
commit after the tag is the intake-hardening merge, which touches no promoted
payload file. The GitHub release carries **no uploaded assets**; its
auto-generated source archive audits identically to the tag tree. The defect was
published, not introduced afterwards.

### 2. Timeline

| Commit | PR | Date | `acfe` manifest state |
| --- | --- | --- | --- |
| `56a72f0` | — | — | 20/20 match |
| `1a919aee0f24adb17fa6345fc15a874d1e3a4c29` | #18 | 2026-06-22 | 20/20 match — last full manifest regeneration (`generated_at: 2026-06-22`) |
| `74682fbe6a1a3933ff1d81ed3f2be5e2ce165d29` | #19 | 2026-06-23 | 22/22 match (AT110 added) |
| `d95067c` | #21 | 2026-06-26 | 22/22 match |
| **`ddb88640e61660edc65ebc956b65dcbd6804d2e6`** | **#22** | **2026-06-26** | **0/22 match — defect introduced** |
| `ed60f26f4bdaa885d6a279a20a6e94aa23cd3655` | — | 2026-06-27 | 1/23 match (display shield added with a correct hash) |
| `2eba24ad776096784764cbe91c8176b434dd3bdf` | #48 | 2026-07-28 | 1/23 match — **tagged and released as `v5.2.23`** |
| `1c6d1512670ae61943d08adeab3eb5591f1689ec` | #49 | 2026-08-03 | 1/23 match — verifier added, gap documented |

### 3. Root cause

PR #22 fixed a real ABI defect. `armclang` defaults to `wchar_t = 4` and 32-bit
enum containers, while NSX images and the heliaRT prebuilts use `-fshort-wchar`
and `-fshort-enums`, so linking the old `acfe` archives failed with armlink
`L6242E`. The PR added `ACFE_ABI_FLAGS` to `sdk-intake/build_ambiqsuite.py`,
rebuilt all 22 `acfe` archives, and promoted the rebuilt bytes.

It did not update `artifact-manifest.yaml`. `git log -- artifact-manifest.yaml`
does not list `ddb8864`, and `generated_at` stayed frozen at `2026-06-22` — the
archives were placed into the generated provider tree without the full
`build_ambiqsuite.py` promotion that also republishes the manifest.

Proof, computed against the committed blobs:

- All 22 stale manifest hashes equal `sha256(git show ddb8864~1:<path>)` — the
  pre-ABI-fix archives, exactly.
- All 22 committed archives equal `sha256(git show ddb8864:<path>)` and have not
  changed since.
- `fromelf --decode_build_attributes` (ArmClang 6.24Rel19) reports
  `Tag_ABI_PCS_wchar_t = 2` and `Tag_ABI_enum_size = smallest container` for all
  **23** committed `acfe` archives, and `wchar_t = 4` / 32-bit enums for the
  pre-fix blobs.

So the committed archives are the good ones and the manifest describes the
discarded ones. The 23rd entry, `apollo4p_evb_disp_shield_rev2`, was added later
by `ed60f26f4bdaa885d6a279a20a6e94aa23cd3655` together with its own correct hash,
which is why exactly one `acfe` entry always matched.

### 4. Why nothing caught it

- No test compared the real committed payload against the real committed
  manifest. `tests/test_intake_workflow.py` exercises `verify_artifact_hashes`
  only against synthetic fixtures in temporary directories, and
  `tests/test_manifests.py` compares provider metadata fields, not hashes. CI was
  green throughout.
- `release/qualification-5.2.23.md` records promoted artifacts as
  *"Recorded as built ... These archives were not rebuilt during release
  preparation"*, and `docs/release-process.md` step 3 asks for a manual hash
  comparison. The step existed; nothing mechanized it.
- `build_ambiqsuite.py --promote-only` deliberately reuses an existing
  `manifest.yaml`, and `promote_provider_payload` copies archives and manifest
  without re-verifying that the manifest describes the archives it just wrote.
- `release/source-ownership.yaml` marks `modules/nsx-ambiqsuite/sdk` as generated
  with `direct_edit: forbidden` and `update_mechanism:
  sdk-intake/build_ambiqsuite.py`. Both `ddb8864` and `ed60f26` edited that tree
  surgically instead of republishing it. The policy existed; nothing enforced it.

### 5. Ownership

| | |
| --- | --- |
| Affected path | `modules/nsx-ambiqsuite/sdk` |
| Ownership class | `upstream-derived-generated`, `direct_edit: forbidden` (`release/source-ownership.yaml`, entry `generated-ambiqsuite-provider`) |
| Update mechanism | `sdk-intake/build_ambiqsuite.py` |
| Provenance record | `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml` |
| Introducing commit author/merger | `apage224` (PR #22) |
| Releasing PR | #48, merged by `apage224` |

## Impact

**Functional: none.** Consumers of `v5.2.23` received the ABI-correct archives.
The ArmClang link smoke recorded in `release/qualification-5.2.23.md` passed
because it linked those archives. No consumer needs to change code, and no
armclang build behaves differently in `5.2.24`.

**Integrity and supply chain: high.** The artifact manifest is the payload's
provenance record. Any consumer, SBOM check, or downstream CI that verified
`v5.2.23` saw 22 archives whose hashes did not match their recorded values — the
same signal a tampered payload produces. Distinguishing the two required this
investigation.

**Blast radius:** all 8 `acfe` HAL archives and 14 of 15 `acfe` BSP archives, on
every `acfe`/armclang-consuming SoC and board in the distribution. `gcc` and
`atfe` consumers were never affected.

## Full ACfE Entry Table

Sizes are the committed archive sizes at `v5.2.23`.

| Part / board | Promoted archive | v5.2.23 manifest sha256 (stale) | Actual committed sha256 | Bytes |
| --- | --- | --- | --- | --- |
| `apollo330P` | `lib/acfe/apollo330P/libam_hal.a` | `3b616580667b8a2c0780a61446d4b809d4e1df78425c1c2a9a6b07a992d94adf` | `32b9a2f0f8a629b5802a12d2ed77d9273f19b761f1cf964b49135b830ef53a36` | 529490 |
| `apollo510` | `lib/acfe/apollo510/libam_hal.a` | `921af557d811a83625f5f4a32a96e0d3f734c029ada3fb7c690e65d7e2b42d5d` | `ea395662427bf9aa2eab4807263d773fb26421e47a99fef5b958e5ca192c2329` | 582870 |
| `apollo510L` | `lib/acfe/apollo510L/libam_hal.a` | `c852d3098cb377c4190eed904902673b8d6e266e428e963e28ce626c5837d689` | `996573b2d759387214ab300070396f1fe6e57cccc5578e40eda61895393d18aa` | 544114 |
| `apollo4l` | `lib/acfe/apollo4l/libam_hal.a` | `54a9229f19fdcdd36842ead40393054de61ef0e39f2289603a2c2eeb7e9af0fd` | `12a3bbf787209ca22fc6a23f86b63fd8c0f1003d522250df848bffcb6925f5ab` | 318358 |
| `apollo4p` | `lib/acfe/apollo4p/libam_hal.a` | `515091ad78be3ed432124a4442fbccb2508f8c7f33b9eeee71b91c0a30336fe5` | `6c8fb86c5646b55c68094a381f3b982800b2b52212c870d1d737917741b64c0f` | 390632 |
| `apollo3` | `lib/acfe/apollo3/libam_hal.a` | `c98df7defb8b61bcb9b3216d8542dfd4cee03fba52ccc4613fbbe22cb7dbc5c7` | `058d3b8380cab57fd25c9cd0a0b36659202e426e860e3bb5e0e356a1b312f1cd` | 280936 |
| `apollo3p` | `lib/acfe/apollo3p/libam_hal.a` | `639c311762cc411e074acaaed78d64bcdc61159cdbb81c63b918483569ed1bef` | `ff6a7c5b8cd777712c1239374d4a5cee2ddbb45b88e853df4bda66948ccbf203` | 278146 |
| `atomiq110` | `lib/acfe/atomiq110/libam_hal.a` | `5491fc555d0dca9763381ddb282c0a03fd94aedb4c2beceb9f33701da38eee9b` | `dcf06b02396335e8c5053730f390794dcfa06241a3f8b93807ce96abf0ac56aa` | 446076 |
| `apollo330mP_evb` | `lib/acfe/apollo330P/apollo330mP_evb/libam_bsp.a` | `4365a5f3c4939775782f507d3e002dfa9656fa49c167f1a4675c2272a315a47b` | `a911d857403a10c6a5873eec1f5efa5eb47a272625247af32e45d774dcaa75b2` | 45396 |
| `apollo510_evb` | `lib/acfe/apollo510/apollo510_evb/libam_bsp.a` | `d7ef0a0caaedb641035e4d03def5a2dbcf59919bb8c59cac518a20a2c3b038b4` | `443d1258fcbf44b7c3212b779805f03a353d2898e08425dbd75b35e9791e82cd` | 76662 |
| `apollo510b_evb` | `lib/acfe/apollo510/apollo510b_evb/libam_bsp.a` | `711f60daa82ee11e18f49355c6d211a37e47d556ee85d03a9ab59c7427a09917` | `50dd2d881e3d11b5b3599b4d465154628399f6db25d86a4fbb3db8eb293a5670` | 97360 |
| `apollo510dL_evb` | `lib/acfe/apollo510L/apollo510dL_evb/libam_bsp.a` | `dff722bba938e335540ae86c813adedfda7c1e173ca9301912ad7f6b435342cb` | `119203adb4ebcf3d06b095b575f628d41ce57f85190a9f27c643f313302f50bd` | 51256 |
| `apollo4l_evb` | `lib/acfe/apollo4l/apollo4l_evb/libam_bsp.a` | `816d214003488b3143ea29754f80b68bae6f3fbecdbe1b4ae48d01aa95544f8e` | `e3bda1a331dba4d6dca4fee56f9cc20127ec75f0bba34a39039270d2501dc527` | 54356 |
| `apollo4l_blue_evb` | `lib/acfe/apollo4l/apollo4l_blue_evb/libam_bsp.a` | `d2346932555d41d8c8379153f9a364870b182e3b57725d49fd68a658c27250fa` | `541075a5d34d29fae80f923d9e24ecd969818e5cdaa1e0cf3c596b78855a9d1c` | 53880 |
| `apollo4p_evb` | `lib/acfe/apollo4p/apollo4p_evb/libam_bsp.a` | `af5d93fd5c794dc3d545e69fb787fb36d0bc4a2a73c70a217c30698d130daa04` | `c2f8adc233a552aa97a4c2dc8f28f3320fc30fabc810a6df6e3ccd2420cdce07` | 61510 |
| `apollo4p_blue_kbr_evb` | `lib/acfe/apollo4p/apollo4p_blue_kbr_evb/libam_bsp.a` | `97512c2f66a4b6bc37e0df5cfcb62255290c03281b173c5d5d2595de17cafb73` | `4b2e140e886d79f98b728d1078fcb1a09139b148eb0ff34cfeae29a5f2847177` | 53102 |
| `apollo4p_blue_kxr_evb` | `lib/acfe/apollo4p/apollo4p_blue_kxr_evb/libam_bsp.a` | `160d0d5a383d9b4511d754943db9c4f67c4b9028ca068c36d94697acb0d06236` | `9145266a3a079b34079f0fb25badb502b8f162eb7588d01565bc5a52cc060ae7` | 60270 |
| `apollo4p_evb_disp_shield_rev2` *(already correct)* | `lib/acfe/apollo4p/apollo4p_evb_disp_shield_rev2/libam_bsp.a` | `d595278b8cdbdca786e5762a9b7c077eed8b8f19b74c39291977e5c64b0171e0` | `d595278b8cdbdca786e5762a9b7c077eed8b8f19b74c39291977e5c64b0171e0` | 63490 |
| `apollo3_evb` | `lib/acfe/apollo3/apollo3_evb/libam_bsp.a` | `2f79ca9b9ddcf349699e65e8c82d74b047b556497c96ba191285487168f2aa6a` | `881b286bcfa9ab837ff0c5b6c09563cc9801efb7b6ff74c1ee428ea08a02fc6c` | 27216 |
| `apollo3_evb_cygnus` | `lib/acfe/apollo3/apollo3_evb_cygnus/libam_bsp.a` | `bb862bfb2461053630215cc730e67cbc7d8d954f6a861285ea8d366100357218` | `1d77c323e5ee4c0be1bb890069e85a93f670c144da3d2040a38e17bba242fa39` | 30738 |
| `apollo3p_evb` | `lib/acfe/apollo3p/apollo3p_evb/libam_bsp.a` | `4de6bcd910b0267f55221db7afd0a53da6326b7304606098aa99a7bca0ef9530` | `3723d84df72d419c6074e4f4c7ba95a344bd6babb691f8753299c7b3f111a565` | 32298 |
| `apollo3p_evb_cygnus` | `lib/acfe/apollo3p/apollo3p_evb_cygnus/libam_bsp.a` | `27b96f26d816336f402e3185eca3ec7e30979b7446ca940a9b828fe118f43298` | `8ac162a5c0ce28b051202d80a356bf18f731a8b6119d26f30221152fd9ac7e3f` | 39824 |
| `atomiq110_fpga_turbo` | `lib/acfe/atomiq110/atomiq110_fpga_turbo/libam_bsp.a` | `9210306bf8230358ab120aabe30df35b4b6ed14a41ba26216a3d89c60e1f9daa` | `4a16c7c7f788da0e54fb8620f6078d73271869a907c8031ebd777e962f8eec05` | 50052 |

## Remediation

### Decision

**Manifest correction published as a new patch distribution version, `5.2.24`.**

The three alternatives were rejected on the evidence:

- *Documentation only.* PR #49 already documented the gap qualitatively, but the
  manifest would stay factually wrong, `verify-baseline` would keep failing on
  `main`, and every future consumer integrity check would keep tripping. Leaving
  a knowingly incorrect integrity record in place is not an acceptable end state.
- *Rebuild the archives and release that.* The committed archives are already the
  correct, ABI-validated, link-smoke-passed builds. Rebuilding would require
  proprietary AmbiqSuite source at `caaf5af86087881647f56c70646c748d40c86e23`
  plus the licensed ArmClang toolchain on internal infrastructure, would produce
  different bytes than the validated ones for no functional gain, and would
  discard the existing qualification evidence.
- *A minor or major version.* No API, payload, source identity, or descriptor
  changed. `docs/versioning.md` requires a new distribution version for any
  change to generated output, which a patch version satisfies.

`docs/versioning.md` already states the governing rule: *"A released version is
never rebuilt in place. ... If a release needs correction, publish a new
distribution version."*

### What `5.2.24` changes

1. **`modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml`** — 22 `acfe` `sha256`
   scalars corrected to the archives actually shipped, plus `abi_cflags` on the
   `acfe` toolchain block. **No archive bytes change.** The binary payload of
   `5.2.24` is identical to `5.2.23`.
2. **`sdk-intake/build_ambiqsuite.py`** — `TOOLCHAIN_ABI_CFLAGS` is emitted into
   generated manifests, so ABI-affecting flags stay recorded provenance across
   future intakes rather than being invisible build detail.
3. **`tests/test_artifact_baseline.py`** — the missing guard. It runs the shipped
   verifier over the real promoted payload, independently re-derives the same
   property without importing the verifier, and rejects any committed archive the
   manifest does not declare. These tests fail on `2eba24ad` and `1c6d151` and
   pass on `5.2.24`.
4. **`release/nsx-ambiq-sdk-5.2.24.yaml`** — maps the distribution version to the
   AmbiqSuite payload `stable-2026.06.18` /
   `caaf5af86087881647f56c70646c748d40c86e23`, and to the actual per-toolchain
   archive provenance including the `acfe` rebuild commit and ABI flags.

Every corrected hash is mechanically derived and independently checkable:

```sh
# each corrected acfe value equals the archive introduced by ddb8864
git show ddb8864:modules/nsx-ambiqsuite/sdk/lib/acfe/apollo510/libam_hal.a | shasum -a 256

# and the whole payload now verifies
python sdk-intake/intake_workflow.py verify-baseline --train stable
```

### Reproduction boundary

Everything in this report was reproduced from committed material only: git
blobs, `hashlib`, and read-only ArmClang `fromelf` inspection of archives
already in the repository. No proprietary AmbiqSuite source, credentials, or raw
vendor input were used or are required.

Re-deriving the archives from AmbiqSuite source is **not** reproducible here and
was not attempted. It needs read-only access to the proprietary tree at
`caaf5af86087881647f56c70646c748d40c86e23` plus the licensed ArmClang, ATfE, and
GCC toolchains recorded in the manifest, on controlled internal infrastructure —
the manual/internal boundary described in
[`intake-hardening.md`](intake-hardening.md). This report therefore proves *which
bytes were published and when*, not that those bytes are the unique output of
that source.

### Follow-ups

- Route future payload changes through `sdk-intake/intake_workflow.py`
  `stage` → `diff` → `promote` so archives and manifest are always republished
  together, instead of editing the generated tree in place.
- The BLE Device Information Service default firmware-revision string still
  reads `5.2.23` in `modules/nsx-ble/src/ns_ble.c` and in third-party
  `svc_dis.c`. It is an overridable runtime default rather than release
  metadata, and one copy is `direct_edit: restricted` third-party source, so it
  is intentionally out of scope here and recorded under `known_deviations` in
  `release/nsx-ambiq-sdk-5.2.24.yaml`.
