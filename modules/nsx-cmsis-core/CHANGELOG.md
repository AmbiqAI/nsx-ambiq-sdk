# Changelog

## 5.2.23 (2026-05-30)

Initial SDK-bundled release for the AmbiqSuite R5.2 NSX module train.

- Imports CMSIS-Core(M) v6.1 headers from
  [ARM-software/CMSIS_6 v6.2.0](https://github.com/ARM-software/CMSIS_6/releases/tag/v6.2.0).
- Provides `nsx::cmsis_core` INTERFACE target exporting the `Include/` directory.
- M-profile only (`a-profile/`, `r-profile/`, and `core_ca.h` are omitted).
