# Bundled Artifacts

This directory is reserved for redistributable artifacts produced from or
approved alongside SWS AmbiqSuite SDK drops.

Raw AmbiqSuite source drops do not belong here. Stage raw drops under
`sdk-intake/local/drops/` and use them as intake/build inputs.

Expected artifact shape:

```text
artifacts/
    ambiqsuite/<version>/
        include/                 # approved redistributable headers, if any
        lib/<part>/libam_hal.a
        lib/<part>/<bsp>/libam_bsp.a
        manifest.yaml
```

The manifest should record:

- source SDK drop name and version;
- source SDK checksum;
- build toolchain and version for generated libraries;
- included parts and boards;
- license/release approval status;
- generated artifact checksums.

CMake descriptors should refer to this artifact layout when building from a
released NSX bundle, and to `sdk-intake/local/` only during local intake or
internal validation.
