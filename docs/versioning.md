# Versioning Policy

## Distribution Version

`5.2.30` is the proposed `nsx-ambiq-sdk` distribution release version. The
root package and every manifest under `modules/` use this version because the
repository is released as one coordinated distribution.

Every distribution release maps to exactly one promoted AmbiqSuite payload,
and its release manifest records where each part of that payload came from.
For `5.2.23` and `5.2.24` the whole payload is one source identity: snapshot
`stable-2026.06.18`, source ref `stable`, source commit
`caaf5af86087881647f56c70646c748d40c86e23`; `5.2.24` republishes the identical
binary payload with a corrected artifact manifest.

Releases `5.2.25` through `5.2.28` carried a mixed-source provider payload.
Their immutable release records retain that provenance and its limitations.

`5.2.29` refreshes the provider headers and all 73 HAL/BSP archives from
prepared source `6a2a0c08af553e33716f2e251cb74aa370e2ae0f`. That source is
upstream `3cfb7d50718e6fb016f1a0128121cb40598109d5` plus two reviewed AT110
XSPI source patches. Four separate payload patches apply after staging.
Cordio/uecc remains a separately versioned vendored snapshot with documented
NSX edits; the new provider identity does not imply a full Cordio refresh.

`5.2.30` replaces the 73 archives with a cumulative build that also removes
the invalid vendor `_sbrk` stub. The reviewed nonarchive provider payload
remains byte-identical to 5.2.29; full re-staging cleanup is tracked in #200.
The manifest distinguishes the new prepared archive source from retained
source/header provenance.

The distribution version does not assert an official numbered AmbiqSuite
release. The source identity in the release manifest and promoted artifact
manifest is authoritative.

## Independent API Versions

Public module API versions are independent of the distribution version. For
example, `NSX_PSRAM_V1_0_0` identifies the `nsx-psram` API contract; it does
not become `5.2.29` and does not create another distribution version axis.
Board descriptor manifests also version independently and intentionally remain
at `0.1.0`; that version describes the descriptor contract, not the bundled
SDK distribution implementation.

## Change Rule

Any change to the promoted AmbiqSuite payload, source identity, NSX patch,
wrapper, descriptor, generated output, or bundled artifact requires a new
distribution version. A released version is never rebuilt in place.

Release tags use `v<distribution-version>`, point to exactly one commit, and
are immutable. Do not move, replace, or force-update a published tag. If a
release needs correction, publish a new distribution version.

This applies to provenance metadata, not just binaries. `5.2.24` corrects
`sha256` records in `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml` without
changing a single archive byte, and is still a new distribution version because
the artifact manifest is generated output inside the promoted payload. See
[`acfe-artifact-manifest-forensics.md`](acfe-artifact-manifest-forensics.md).
