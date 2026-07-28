# Versioning Policy

## Distribution Version

`5.2.23` is the current `nsx-ambiq-sdk` distribution release version. The
root package and every manifest under `modules/` use this version because the
repository is released as one coordinated distribution.

Every distribution release maps to exactly one promoted AmbiqSuite payload
and source identity. For `5.2.23`, that identity is snapshot
`stable-2026.06.18`, source ref `stable`, source commit
`caaf5af86087881647f56c70646c748d40c86e23`.

The distribution version does not assert an official numbered AmbiqSuite
release. The source identity in the release manifest and promoted artifact
manifest is authoritative.

## Independent API Versions

Public module API versions are independent of the distribution version. For
example, `NSX_PSRAM_V1_0_0` identifies the `nsx-psram` API contract; it does
not become `5.2.23` and does not create another distribution version axis.
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
