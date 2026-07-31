# Manual Release Process

This is the lightweight process for the first golden-baseline release.
Automated staging, promotion, patch-series management, and containerized
intake redesign are out of scope.

For subsequent drops, `sdk-intake/intake_workflow.py` (see
[`docs/intake-hardening.md`](intake-hardening.md)) provides an optional
staged-promotion layer -- staging, a reviewable diff, ownership/hash
verification, and an explicit atomic promotion step -- built on top of the
same `build_ambiqsuite.py` promotion logic this process already uses. It does
not change any step below on its own; a maintainer still drives promotion,
review, and the tag/release steps explicitly.

## Prepare And Qualify

1. Fetch `origin` and create a clean release branch from the latest
   `origin/main`.
2. Confirm the root version, all manifests under `modules/`, and
   `release/nsx-ambiq-sdk-<version>.yaml` agree.
3. Compare the release and provider provenance with
   `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml`, including the exact
   source ref, commit, snapshot identity, toolchain status, and artifact
   hashes.
4. Review generated/vendor and NSX-owned changes according to
   `docs/source-ownership.md`. Do not hand-edit generated provider output.
5. Update the changelog and qualification report. Keep descriptor-only,
   experimental, untested, and feature-only hardware evidence explicit.
6. Run the exact CI checks:

   ```sh
   uv sync --group ci
   uv run --group ci pre-commit run --all-files
   uv run --group ci python -m py_compile sdk-intake/build_ambiqsuite.py
   uv run --group ci python -m pytest
   ```

7. Run available CMake link smokes from `docs/toolchain-smoke.md` and record
   toolchain/board coverage. A missing licensed or local toolchain is a
   recorded gap, not an implied pass.
8. Review the complete diff and verify the worktree is clean after approved
   commits.

## Publish With Approval

1. Merge only after release approval, then resolve the release commit with
   `git rev-parse HEAD`.
2. Create annotated tag `v5.2.23` at that exact commit only after explicit
   approval. Never move or replace the tag.
3. Verify the tag target with `git rev-list -n 1 v5.2.23`; this is the
   resolution mechanism for the release manifest's pre-tag commit field.
4. Publish the GitHub release from that tag with title
   `nsx-ambiq-sdk 5.2.23 — AmbiqSuite stable-2026.06.18`.
5. Attach only approved release material and verify no credentials or raw
   proprietary AmbiqSuite source are present.

## Intake Security Requirements

- AmbiqSuite access must be read-only; use the least-privileged credentials
  needed to resolve the approved ref/commit.
- Never put credentials, activation data, or raw proprietary source into
  source control, logs, images, caches, or release artifacts.
- Record the exact source commit and cryptographic artifact hashes.
- Run proprietary or licensed automation on controlled internal/self-hosted
  infrastructure when public runners cannot meet access and licensing
  requirements.
