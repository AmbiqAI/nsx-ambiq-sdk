# nsx-cmsis-core

Standalone packaging of the **ARM CMSIS-Core(M) v6.1** headers (from the
[CMSIS_6 v6.2.0](https://github.com/ARM-software/CMSIS_6/releases/tag/v6.2.0)
release) as an NSX module.

This module owns the CMSIS Core headers used by NSX modules and applications.
Consumers link against `nsx::cmsis_core` for headers such as `core_cm55.h`,
`cmsis_compiler.h`, and `m-profile/armv8m_pmu.h`.

## What's inside

`Include/` mirrors the upstream CMSIS_6 `CMSIS/Core/Include/` tree, restricted
to the M-profile parts (a-profile/r-profile/`core_ca.h` are omitted):

- Top-level: `core_cm{0,0plus,1,3,4,7,23,33,35p,52,55,85}.h`,
  `core_sc{000,300}.h`, `core_starmc1.h`, `cmsis_{version,compiler,gcc,
  armclang,iccarm,clang}.h`, `tz_context.h`
- `m-profile/`: `armv7m_cachel1.h`, `armv{7,8}m_mpu.h`, `armv8m_pmu.h`,
  `armv81m_pac.h`, per-compiler intrinsics shims

## Consuming this module

```cmake
find_package(nsx_cmsis_core REQUIRED)
target_link_libraries(my_module PUBLIC nsx::cmsis_core)
```

After that, headers are accessible by their normal CMSIS paths:

```c
#include "core_cm55.h"
#include "m-profile/armv8m_pmu.h"
```

CMSIS 6 M-profile PMU, MPU, cache, and PAC headers live under `m-profile/`.

## License

Apache-2.0. Upstream license preserved verbatim in [`LICENSE`](LICENSE).
