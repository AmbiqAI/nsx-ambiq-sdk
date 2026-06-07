# Upstream Licensing Notice

`nsx-ambiqsuite-r4` contains imported AmbiqSuite R4-era SDK content under
`sdk/`.

This repo does not apply a new NSX-authored root license over the imported SDK
payload. Licensing for the imported SDK content remains governed by the
upstream AmbiqSuite files and notices included in the SDK snapshot.

Important notes:

- many imported AmbiqSuite source files include Ambiq BSD-style license headers
- the upstream AmbiqSuite software agreement is carried under `sdk/docs/licenses/`
- third-party software bundled in the SDK remains subject to its own license
  terms, also carried under `sdk/docs/licenses/`
- prebuilt binary archives in `sdk/lib/` are imported or generated from the
  upstream AmbiqSuite SDK baseline listed in `sdk/artifact-manifest.yaml`

This repo exists to preserve and package upstream SDK snapshots for NSX use. It
should be interpreted as a curated import of upstream content, not as a
relicensing of that content by NSX.
