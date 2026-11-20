# License and notice index

The root [LICENSE](../LICENSE) and [NOTICE](../NOTICE) describe the scope for
Ambiq-authored NSX integration. The texts in this directory are copies of
licenses already carried by components. Retain the original file and
component notices when distributing those components.

| Component or path | License or notice location |
| --- | --- |
| Ambiq-authored NSX integration | [AmbiqSuite BSD terms](../LICENSE), with existing module [BSD-3-Clause notices](BSD-3-Clause.txt) retained. |
| AmbiqSuite provider under `modules/nsx-ambiqsuite/sdk/` | [AmbiqSuite agreement](../modules/nsx-ambiqsuite/sdk/docs/licenses/LICENSE.rtf), [Ambiq Software License Terms](../modules/nsx-ambiqsuite/sdk/docs/licenses/Ambiq-Software-License-Terms.pdf), [upstream licensing notice](../modules/nsx-ambiqsuite/UPSTREAM-LICENSE-NOTICE.md), and [bundled notices](../modules/nsx-ambiqsuite/THIRD-PARTY-PAYLOAD.md). |
| CMSIS-Core under `modules/nsx-cmsis-core/` | [Arm's Apache-2.0 license](../modules/nsx-cmsis-core/LICENSE) and file notices. |
| FreeRTOS kernel | [Upstream MIT license](../modules/nsx-freertos/sdk/third_party/FreeRTOS-Kernel/LICENSE.md). The NSX wrapper carries a [BSD-3-Clause notice](../modules/nsx-freertos/LICENSE). The [CM4F handlers](../modules/nsx-freertos/src/nsx_freertos_handlers_cm4f.c) and [configuration template](../modules/nsx-freertos/templates/FreeRTOSConfig.h.template) carry MIT notices. |
| TinyUSB | [Upstream MIT license](../modules/nsx-ambiq-usb/sdk/third_party/tinyusb/source/LICENSE). |
| AmbiqSuite-derived USB integration | In-file notices under [`modules/nsx-ambiq-usb/sdk/`](../modules/nsx-ambiq-usb/sdk), outside the TinyUSB subtree. |
| Cordio | In-file notices under [`modules/nsx-cordio/sdk/third_party/cordio/`](../modules/nsx-cordio/sdk/third_party/cordio), including [Apache-2.0](Apache-2.0.txt) headers and Software License Agreement references in the [WSF FreeRTOS port](../modules/nsx-cordio/sdk/third_party/cordio/wsf/sources/port/freertos) and other legacy files. Consult the notice on each source file. |
| micro-ECC | [Upstream BSD-2-Clause license](../modules/nsx-cordio/sdk/third_party/uecc/license.txt). |
| AmbiqSuite-derived PSRAM device drivers | In-file notices under `modules/nsx-psram/src/<family>/devices/`. |
| Unity hardware tests | [Upstream MIT license](../tests/hardware/vendor/unity/LICENSE.txt). |
