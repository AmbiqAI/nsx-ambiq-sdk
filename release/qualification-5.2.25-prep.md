# nsx-ambiq-sdk 5.2.25 — release-prep note (DRAFT, not a qualification record)

Status: **prep**. This file reserves the 5.2.25 record for content that landed
on `main` after the immutable `v5.2.24` tag was published. Per the policy
recorded in `release/nsx-ambiq-sdk-5.2.24.yaml` (`immutability_note`),
released records are never amended in place — additions and corrections ship
as a new distribution version. The formal `nsx-ambiq-sdk-5.2.25.yaml` and
`qualification-5.2.25.md` are produced by the release-prep process at tag
time (see the 5.2.23 → 5.2.24 precedent).

## Content queued for 5.2.25 (merged after v5.2.24)

- **atomiq110 platform support** (PR #46): regenerated atomiq110 HAL/BSP
  artifacts from the `npu-drop-2026.07.09` payload (source commit
  `960624ee18afd9fbf5db957371c92c828bf6a721` — per-artifact provenance is
  recorded in `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml` comments),
  the `nsx-power`/`nsx-core` atomiq110 backends, and the new `nsx-npu`
  module (Ethos-U85 glue).
- **External module dependency**: `nsx-npu` requires `nsx-ethos-u-driver`,
  which is deliberately **not** part of this distribution — it is resolved
  from `AmbiqAI/nsx-ethos-u-driver` (pinned at
  `nsx-ethos-u-driver-v0.1.2`) by the neuralspotx registry at
  workspace-assembly time. The 5.2.25 record should state this scope
  boundary explicitly.
- Qualification scope: atomiq110 remains **experimental** (FPGA bring-up;
  `atomiq110_fpga_turbo`) and is expected to stay outside the qualified
  archive scope for 5.2.25 unless silicon qualification lands first.

## Recorded payload divergence: atomiq110 INTERNAL-marker sanitation (issue #52)

The atomiq110 headers on `main` **diverge from the `npu-drop-2026.07.09`
payload** and the 5.2.25 record must say so. `npu-drop-2026.07.09` is a raw
engineering drop: upstream's release tooling strips vendor
`#### INTERNAL ####` fences and the dead content between them before
publishing, and that step had not been run. The divergence is a one-time,
**deletion-only** sanitation performed by `sdk-intake/internal_markers.py` —
988 lines removed across 23 files under
`modules/nsx-ambiqsuite/sdk/mcu/atomiq110`, zero lines added or modified.
Only comment-only marker lines, INTERNAL block bodies with no live
preprocessor tokens, `#if 0 … #endif` regions inside a fence, and blank lines
at a deletion seam were deleted; any block still holding a live declaration
kept its body and its fence verbatim, so nothing visible to the compiler that
built the prebuilt archives changed. The result is tool-reproducible: the
scrubber run over the pre-scrub tree yields the committed tree byte-for-byte,
and re-running it over the committed tree is a no-op. **Per-artifact
provenance strings and `artifact-manifest.yaml` are retained unchanged, and
the prebuilt archives are not affected — no artifact hash changes.** The
scoped ownership exception is recorded on the `generated-ambiqsuite-provider`
entry in `release/source-ownership.yaml`; future payloads are sanitized while
staged, so no comparable divergence should recur.

## How the 5.2.24 records are treated

`v5.2.24` predates all of the above. The **qualification report**
(`qualification-5.2.24.md`) is frozen prose describing the qualified tagged
content and is left exactly as released — amending it on `main` would
recreate the record/payload disagreement class that
`docs/acfe-artifact-manifest-forensics.md` documents (and that 5.2.24 itself
was cut to correct for 5.2.23). The **release manifest's**
`payload_provenance.toolchain_trains[].archives_introduced_in` lists are the
one deliberate exception: they are a living, machine-verified attribution
ledger (`tests/test_release_metadata.py::
test_archives_introduced_in_matches_git_history` checks them against git),
so the atomiq110 regeneration commit `960624ee1` is recorded there — and
those entries carry forward into the formal 5.2.25 manifest at release-prep
time.
