# Intake Hardening: Staged AmbiqSuite Intake Workflow

`sdk-intake/intake_workflow.py` is a maintainer-facing hardening layer on top
of `sdk-intake/build_ambiqsuite.py`. It exists so that regenerating the
`nsx-ambiqsuite` provider payload never has to directly overwrite the
committed, promoted tree in one destructive step. It does not replace
`build_ambiqsuite.py`; it consumes artifacts that tool already built and adds
staging, a reviewable diff, boundary/hash verification, an optional patch
hook, and an explicit atomic promotion step around the existing promotion
logic.

This document is for maintainers preparing a new AmbiqSuite drop or auditing
the current one. It does not change anything about the promoted payload;
running these commands read-only (`diff`, `verify-hashes`, `verify-baseline`,
`verify-ownership`) is always safe.

## Why Staging Instead Of Direct Promotion

`build_ambiqsuite.py --promote` / `--promote-only` still work exactly as
before and still write directly into `modules/nsx-ambiqsuite/sdk/` — that
entry point is unchanged and remains available for the existing manual
release process in `docs/release-process.md`.

`intake_workflow.py stage` instead promotes the same curated payload into a
throwaway directory under `sdk-intake/local/staging/<train>/<version>/sdk/`
(already excluded from version control by `sdk-intake/local/.gitignore`).
Nothing under `modules/` is touched until an explicit, separate `promote`
step, which itself re-verifies the staged tree and requires `--yes`.

## Workflow

```text
build_ambiqsuite.py  ──build──▶  artifacts/ambiqsuite/<train>/<version>/
                                          │
                                          ▼
intake_workflow.py stage  ──promote-to-staging + patch hook + hash check──▶
                                 sdk-intake/local/staging/<train>/<version>/sdk/
                                          │
                          intake_workflow.py diff (reviewable, read-only)
                                          │
                          intake_workflow.py promote --yes (atomic swap)
                                          ▼
                                 modules/nsx-ambiqsuite/sdk/
```

1. Build artifacts as usual (unchanged):

   ```sh
   python sdk-intake/build_ambiqsuite.py \
     --toolchain all \
     --ambiqsuite-repo /path/to/ambiqSuite \
     --source-ref stable \
     --atfe-root /path/to/ATfE \
     --acfe-root /path/to/ACfE
   ```

   This step is unchanged and still only writes under
   `artifacts/ambiqsuite/<train>/<version>/`; it never touches
   `modules/nsx-ambiqsuite/sdk/`.

2. Stage the curated payload from those built artifacts:

   ```sh
   python sdk-intake/intake_workflow.py stage \
     --train stable \
     --version stable-2026.06.18 \
     --ambiqsuite-repo /path/to/ambiqSuite \
     --source-ref stable
   ```

   This refuses to run (fails closed) unless the full train's artifact set is
   already complete on disk for that version — the same completeness check
   `build_ambiqsuite.py --promote` uses. It generates into staging, applies
   any patch hook (see below), then recomputes every promoted HAL/BSP
   archive's sha256 and compares it against the freshly generated
   `artifact-manifest.yaml`; any mismatch or missing archive aborts staging
   before anything is proposed for promotion.

3. Review a reviewable diff against the currently promoted tree:

   ```sh
   python sdk-intake/intake_workflow.py diff \
     --train stable --version stable-2026.06.18
   ```

   Added/removed files are listed with their hash; modified text files (headers,
   system sources, license/docs) get a unified diff; modified binaries (prebuilt
   `.a` archives) get a hash-only line. This step is entirely read-only.

4. Promote only after the diff has been reviewed and approved:

   ```sh
   python sdk-intake/intake_workflow.py promote \
     --train stable \
     --staged-dir sdk-intake/local/staging/stable/stable-2026.06.18/sdk \
     --yes
   ```

   `promote` re-verifies the staged tree's artifact hashes and the
   generated-provider ownership boundary before doing anything, then performs
   an atomic directory swap: it copies the staged tree into a temporary
   sibling, renames the current provider tree aside as a backup, renames the
   temporary tree into place, and only removes the backup after the swap
   succeeds. If any step fails, it rolls back to the pre-promotion state
   rather than leaving `modules/nsx-ambiqsuite/sdk/` missing or half-written.

   Promotion does not bump the distribution version, update
   `release/nsx-ambiq-sdk-<version>.yaml`, or touch tags/releases — that
   remains the separate, human-approved process in
   [`docs/release-process.md`](release-process.md).

## Boundary And Manifest Verification

Every `stage` and `promote` run checks
[`release/source-ownership.yaml`](../release/source-ownership.yaml)'s
`generated-ambiqsuite-provider` entry and refuses to proceed unless:

- it is marked `generated: true` and `direct_edit: forbidden`, and
- the train's actual provider path (`modules/nsx-ambiqsuite/sdk`) is exactly
  one of that entry's declared `paths`.

This is a fail-closed guard against configuration drift silently promoting
generated output somewhere the ownership inventory does not recognize as
generated. It can be run standalone:

```sh
python sdk-intake/intake_workflow.py verify-ownership --train stable
```

## INTERNAL Engineering-Content Sanitizer

A raw engineering drop fences internal notes — commented-out enums, `#if 0`
experiments, ticket chatter — between `#### INTERNAL BEGIN/END ####` markers
that upstream's release tooling strips before
publishing. `stage` scans the staged payload for those markers and ratchets the
result against [`sdk-intake/internal-marker-baseline.yaml`](../sdk-intake/internal-marker-baseline.yaml),
which records what each historical file is still allowed to carry. Any increase,
or any marker in a file with no entry, fails the stage. The same check runs
standalone and in `tests/test_internal_markers.py`:

```sh
python sdk-intake/intake_workflow.py verify-internal-markers
python sdk-intake/internal_markers.py scan --root modules/nsx-ambiqsuite/sdk \
  --path-prefix modules/nsx-ambiqsuite/sdk
```

Sanitize a drop *at intake*, while it is still staged:

```sh
python sdk-intake/internal_markers.py scrub --root <staged tree>          # dry run
python sdk-intake/internal_markers.py scrub --root <staged tree> --write
python sdk-intake/internal_markers.py update-baseline                     # after the scrub lands
```

The scrub only ever deletes marker lines, INTERNAL blocks whose body has no live
tokens, `#if 0 … #endif` regions inside an INTERNAL block, and blank lines left
at a deletion seam. An INTERNAL block that still holds live declarations keeps
its body verbatim and loses only its fence — a live declaration fenced inside an
INTERNAL block is part of the ABI the drop's prebuilt archives were compiled
against, so removing it post-hoc would desynchronize headers from `libam_hal.a`.
That is why this is an intake-time gate rather than a cleanup pass on a shipped
payload.

A kept block keeps its `BEGIN`/`END` fence as well as its body. Dropping the
fence would be worse than cosmetic: the retained content is still
vendor-internal, and an unfenced remnant is invisible to `scan`, so the ratchet
would lose sight of it permanently. Keeping the fence means the retained lines
stay counted in the baseline, stay reviewable at the next intake, and make the
scrub idempotent — running it again over a scrubbed tree changes nothing, which
is what `tests/test_internal_markers.py` asserts against the promoted tree.

Because counting marker lines cannot see fence *shape* — deleting half of a
`BEGIN`/`END` pair makes the count go *down*, which a ratchet reads as an
improvement — the baseline carries a second list, `unparseable_fences`. It
records files whose fences the block parser refuses (orphan or nested markers, a
marker sharing a line with live code, a non-UTF-8 payload). A newly unparseable
file fails the stage even when its marker count is within budget.

### Scope Boundary: Board BSPs Are Not In This Scrub

The scrub landed for `modules/nsx-ambiqsuite/sdk/mcu/atomiq110` only. Board BSP
headers — notably
`modules/nsx-ambiqsuite/sdk/boards/atomiq110_fpga_turbo/bsp/am_bsp.h` — keep
their markers and are **baselined, not scrubbed**. That is deliberate rather
than an oversight: BSP headers are the interface every board example compiles
against, the BSP sources and their prebuilt `libam_bsp.a` were built from the
drop as shipped, and BSP internal blocks in practice fence pin tables and
peripheral instance maps whose "dead" lines are far more often live than in the
HAL headers. Scrubbing them is a separate change with its own build-and-link
verification, and it happens at the board tree's *next* intake.

### The Legacy Baseline Is A Collapse-Over-Time Plan

The baseline records roughly 2.3k marker lines across ~330 files. That is not a
target to be driven to zero in one pass, and it is not an accepted permanent
state: it is a snapshot of what already shipped. Every one of those files landed
alongside prebuilt archives compiled from it, so a post-hoc scrub of any of them
risks exactly the header/binary desynchronization this gate exists to prevent.
The plan is therefore incremental and mechanical: **each tree is scrubbed at its
own next intake**, while the drop is still staged and before its archives are
promoted, and the baseline is regenerated as part of that intake. The number
collapses tree by tree as SoC payloads are refreshed. What the ratchet
guarantees in the meantime is the direction of travel — the count can only go
down.

### The Baseline Is Review-Gated, Not Tool-Gated

`update-baseline` is self-service by design: anyone can regenerate the file, and
nothing in the tooling prevents an entry's count from being raised. That is not
a hole in the gate — it is where the gate hands off. The control is the **diff**.
Raising a count, or adding a file to `unparseable_fences`, is a visible line in a
pull request that a reviewer has to approve, with the offending `path:line`
output right there in the failing check that prompted it. The tool's job is to
make new internal content *impossible to land silently*; deciding whether a
specific increase is justified is a human review decision, and forcing it
through a reviewed diff is the strongest form that decision can take.

## Artifact Hash Verification And The Golden Baseline

`verify-hashes` recomputes sha256 for every HAL/BSP archive under any SDK tree
(staged or promoted) and compares it against that tree's own
`artifact-manifest.yaml`:

```sh
python sdk-intake/intake_workflow.py verify-hashes \
  --sdk-dir modules/nsx-ambiqsuite/sdk
```

`verify-baseline` is the same check, defaulted at the currently promoted
tree — the reproducible integrity check for an already-published golden
baseline such as `v5.2.24`:

```sh
python sdk-intake/intake_workflow.py verify-baseline --train stable
```

**What this does and does not prove.** This is read-only and requires no
proprietary source: it proves the committed archives still match the hashes
recorded when they were built. It does **not** re-derive the archives from
AmbiqSuite source, and it cannot prove the recorded hashes were correct in the
first place. Full reproduction of a promoted payload requires manual/internal
access to the exact upstream commit
(`caaf5af86087881647f56c70646c748d40c86e23` for `stable-2026.06.18`) plus the
exact licensed toolchains recorded in the manifest, on infrastructure that
meets the read-only-access and no-credential-leakage requirements below. That
step is outside what any automation in this public repository can perform or
claim.

**Resolved: the `v5.2.23` `acfe` manifest gap.** `verify-baseline` did not pass
against `v5.2.23`: 22 of its 23 `acfe` HAL/BSP archives had a sha256 that no
longer matched `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml`. The archives
were correct; the manifest still described the pre-ABI-fix archives replaced by
`ddb88640e61660edc65ebc956b65dcbd6804d2e6`. `gcc` and `atfe` were never
affected.

`v5.2.23` stays published and immutable. The manifest is corrected in
distribution version `5.2.24`, which republishes the identical binary payload.
`verify-baseline` passes from `5.2.24` onward, and
`tests/test_artifact_baseline.py` now runs the same check in CI so the payload
and its manifest can never diverge silently again. The full investigation —
per-artifact hashes, root cause, ownership, impact, and the remediation
decision — is recorded in
[`acfe-artifact-manifest-forensics.md`](acfe-artifact-manifest-forensics.md).

## Patch Hook

`stage` applies an optional, ordered patch hook from
[`sdk-intake/patches/<train>/`](../sdk-intake/patches/README.md) after
generating the staged payload and before hash verification. It is scoped to
exceptional upstream fixes, not a general edit mechanism — see that README
for the patch/sidecar format, ownership metadata requirements, and the
fail-closed reapplication-failure behavior (a patch that no longer applies
cleanly aborts staging with a named owner to follow up with, rather than
silently skipping or partially applying).

## Safe Operation

- **AmbiqSuite access must be read-only.** Use the least-privileged
  credentials needed to resolve the approved ref/commit, matching
  `docs/release-process.md`'s intake security requirements.
- **Local:** point `--ambiqsuite-repo` at a local, read-only clone. Staging
  and diffing never require write access to that clone beyond the worktree
  `build_ambiqsuite.py` already materializes under
  `sdk-intake/local/work/`.
- **Container:** the dev/CI container (`docker/dev-ci.Dockerfile`) is the same
  environment used by the opt-in `Container Validation` workflow; mount the
  AmbiqSuite checkout read-only (`-v /path/to/ambiqSuite:/ambiqsuite-repo:ro`)
  when running `stage` inside it.
- **Internal runner:** ACfE builds and any step touching licensed toolchains
  must run on controlled internal/self-hosted infrastructure, never on public
  runners, matching the existing `run_sdk_build` opt-in job in
  `.github/workflows/container-validation.yml`.
- **No credential or proprietary-source leakage.** Never let raw AmbiqSuite
  source, activation codes, or credentials land in source control, logs,
  container images, caches, or staged/promoted output. `stage` only ever
  writes curated, already-approved-shape output (the same promotion logic
  `build_ambiqsuite.py` uses) into the ignored staging tree; it never copies
  raw upstream source wholesale.
- **Exact source identity.** Every stage/verify command records or checks the
  exact `source_commit` and per-archive sha256 already tracked in
  `artifact-manifest.yaml`; there is no "close enough" match. A version whose
  on-disk artifact set is incomplete for the current train fails closed
  rather than staging a partial payload.

## What This Tool Does Not Do

- It does not build HAL/BSP archives — that remains
  `sdk-intake/build_ambiqsuite.py`'s job.
- It does not bump the distribution version, edit
  `release/nsx-ambiq-sdk-<version>.yaml`, or touch git tags/releases.
- It does not modify `neuralSPOT-X`/`HPX` (out of this repository's scope).
- It does not claim to reproduce a payload from source; see "Artifact Hash
  Verification And The Golden Baseline" above.
