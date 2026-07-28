# Source Ownership

This repository combines generated provider output, upstream-derived
auxiliary copies, and NSX-owned integration. Ownership determines how a path
may be changed; it does not require moving existing trees.

The machine-readable inventory is
[`release/source-ownership.yaml`](../release/source-ownership.yaml).
More-specific inventory entries take precedence over broad NSX wrapper
patterns.

## Generated Provider Payload

`modules/nsx-ambiqsuite/sdk/` is generated, promoted output from
`sdk-intake/build_ambiqsuite.py`. Do not edit it directly. Regenerate and
review the complete payload, artifact manifest, source identity, and hashes.
Raw AmbiqSuite source remains outside the committed repository.

## Upstream-Derived Auxiliary Copies

These are source copies carried for optional modules rather than generated
provider output:

- `modules/nsx-ambiq-usb/sdk/` contains AmbiqSuite USB integration and
  third-party TinyUSB source.
- `modules/nsx-psram/src/*/devices/` contains AmbiqSuite-derived PSRAM device
  drivers.
- Other explicit `modules/*/sdk/third_party/` trees retain their upstream
  ownership and notices.

Direct edits are restricted. Refresh from the identified upstream source
where possible. Any necessary NSX patch must be intentional, reviewable, and
included in a new distribution version; never blur it into a generated
refresh.

## NSX-Owned Integration

NSX owns the wrapper CMake, public APIs, runtime integration, tests, and
module metadata outside the upstream-derived exceptions above. NSX also owns:

- `cmake/socs/` SoC descriptors and facts;
- `boards/` board descriptors and NSX board facts;
- release policy, qualification, and provenance records.

These paths may be edited directly through normal review. They can reference
upstream names and artifacts without becoming upstream-generated source.
