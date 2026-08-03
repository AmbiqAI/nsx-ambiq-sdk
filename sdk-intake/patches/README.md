# Intake Patch Hook

This directory is an optional, ordered patch hook applied by
`sdk-intake/intake_workflow.py stage` after a train's curated payload is
generated into staging and before it is hashed/verified. It exists for
**exceptional** upstream fixes only: a small, explicit, reviewable overlay for
cases where the upstream AmbiqSuite drop needs a correction that cannot wait
for the next drop, applied on top of otherwise-unmodified generated output.

This is not a general mechanism for hand-editing the generated provider
payload. `modules/<module>/sdk/` remains generated, upstream-derived output
(see [`docs/source-ownership.md`](../../docs/source-ownership.md)); a patch
here must be a targeted, well-understood fix to that generated output, not a
substitute for NSX-owned integration code.

## Layout

```text
sdk-intake/patches/<train>/
    001-short-slug.patch    # unified diff (git apply format), applied from
                            # the staged sdk/ directory as the apply root
    001-short-slug.yaml     # required ownership sidecar
```

Patches are applied in filename order (hence the numeric prefix). Each
`*.patch` file **must** have a same-named `*.yaml` sidecar; staging refuses to
run (fails closed) if a patch is missing its sidecar or the sidecar is missing
required fields.

## Sidecar Fields

```yaml
owner: jane.doe          # required: who is responsible for this patch
reason: >                # required: why this patch exists
  Short, specific description of the upstream defect and the fix.
upstream_ref: AMBIQ-1234 # optional: upstream ticket/PR/commit reference
```

## What A Patch May Touch

A patch may not touch anything under `lib/` (the prebuilt HAL/BSP archives)
or `artifact-manifest.yaml`. Those are hash-verified output: allowing a patch
to rewrite either would let a patch make a tampered archive verify against a
manifest it also rewrote. `stage` rejects (fails closed) any patch whose
`git apply --numstat` output touches either before it ever applies anything.
Patches are for generated headers and system sources.

## Ordering And Failure

- Patches apply in ascending filename order via `git apply --check` (dry run)
  followed by `git apply`, invoked with the staged `sdk/` directory as the
  target root and without `--unsafe-paths`, so a patch cannot write outside
  the tree it targets.
- If any patch fails `git apply --check` — most commonly because upstream
  content drifted enough that the patch no longer applies cleanly — staging
  aborts immediately with a clear error naming the patch and its owner. No
  patches are left partially applied, and the staged tree is not treated as
  valid output.
- A failed reapplication means the patch needs maintainer attention (rebase
  it against the new upstream content, or retire it if upstream fixed the
  underlying issue) before staging can proceed again.

## Normal State

This directory should usually be empty (aside from this README and the
per-train placeholder directories). A patch here is expected to be temporary:
once the fix lands upstream and flows through the next AmbiqSuite drop, retire
the patch.
