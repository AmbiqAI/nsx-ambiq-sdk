# nsx-ambiqsuite-r2

`nsx-ambiqsuite-r2` is the raw AmbiqSuite R2 SDK provider repo used by NSX.

This repo keeps the imported vendor SDK under `sdk/` and exposes a thin NSX
provider target through `CMakeLists.txt` and `nsx-module.yaml`.

The promoted provider surface follows the same curation rules as the newer R3,
R4, and R5 tiers:

- CMSIS core headers come from `nsx-cmsis-core`, not the imported vendor tree.
- The provider keeps the staged SDK payload under `sdk/` and does not expose
  legacy nested repo scaffolding.
- Changes to the exposed surface should happen by selecting staged files,
  omitting unsupported content, or adding NSX-owned integration files outside
  the imported vendor tree.

Licensing for the imported SDK content follows the upstream AmbiqSuite terms.
