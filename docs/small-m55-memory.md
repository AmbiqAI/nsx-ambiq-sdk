# Apollo330 Plus and Apollo510 Lite memory layouts

The `apollo330P` and `apollo510L` source directories are **not** full Apollo510
memory maps. Their built-in SBL/default and NBL layouts support the AP330MPA
and AP510DLA derivatives selected by the shipped board descriptors.

| Resource | Start | Exclusive end | Notes |
| --- | --- | --- | --- |
| Physical NVM | `0x00400000` | `0x00600000` | 2 MiB, including radio firmware |
| Application NVM, NBL | `0x00400000` | `0x00594000` | Leaves the radio firmware reservation intact |
| Application NVM, SBL/default | `0x00410000` | `0x00594000` | Also reserves the first 64 KiB for the bootloader |
| DTCM | `0x20000000` | `0x20040000` | 256 KiB; executable code can use `.dtcm_text` |
| Shared SRAM | `0x20080000` | `0x20240000` | 1.75 MiB |
| ITCM at address zero | — | — | Not implemented on these parts |

`NSX_LINKER_PROFILE=itcm` therefore fails at configure time for these two SoCs,
for GCC, ATfE and ArmClang. The unsupported ITCM scripts are removed, rather
than retaining an explicit-script path that can still link into absent RAM.
NBL scripts put code in NVM and optional `.dtcm_text` code in real DTCM, with
the same startup-copy symbols as the default scripts. To use NBL, select the
corresponding `linker_script_nbl.ld`/`.sct` through `NSX_LINKER_SCRIPT`.

The GNU scripts retain the existing 240 KiB TCM allocation and 1 KiB minimum
heap guard. The ArmClang layouts retain separate data, heap and stack regions
inside the 256 KiB aperture. Neither compiler may allocate into the reserved
gap between DTCM and shared SRAM.

## Evidence and conflicting sources

- **Apollo330 Plus Series Datasheet**, DS-A330PS-0p9p0, revision 0.9.0,
  July 2026, Table 4, page 33: no memory at address zero; 256 KiB DTCM;
  1.75 MiB shared SRAM. Application NVM ends at `0x00594000` for Apollo330M
  Plus. The package section identifies AP330MPA as that derivative.
- **Apollo510 Lite Series Datasheet**, DS-A510LS-0p9p0, revision 0.9.0,
  July 2026, Table 4, page 33: the same physical RAM apertures and application
  NVM boundary for Apollo510D Lite, identified as AP510DLA in the package
  section.
- The shipped `cmake/socs/facts/apollo330P.cmake` and `apollo510L.cmake`
  select SEGGER devices `AP330MPA-CBR` and `AP510DLA-CBR` respectively.
- AmbiqSuite `stable` at `b5375f2ab9054d774854fb5c223ef3129f261f42`,
  `mcu/apollo330P_510L/regs/am_reg_base_addresses.h`, explicitly describes no
  ITCM and defines 256 KiB DTCM, 1792 KiB SSRAM and 2048 KiB physical MRAM.
  The corresponding vendored per-part headers agree on these physical sizes.

The SDK's **physical** MRAM size is not a promise that all of it is application
storage. Table 4 separately reserves the upper region for Cortex-M4 firmware;
its boundary varies between the non-radio, BLE and dual-mode derivatives.
The old default scripts also exceeded the application boundary for the shipped
boards, even though they stayed within the physical NVM aperture. Copying those
default sizes into the other profiles would preserve that defect.

These layouts use the AP330MPA/AP510DLA application window, which is also a
conservative window for other derivatives in these two series. A custom board
that uses another derivative can supply an explicitly verified custom linker
script; it must preserve that derivative's bootloader and radio firmware
reservations. Full Apollo510 and Atomiq110 keep their own memory maps and ITCM
profiles.

## Verification

`tests/test_small_m55_memory.py` checks GNU and ArmClang region declarations,
rejects selecting the unsupported profile, and links real Arm object files
against both SoCs' GNU SBL/NBL scripts. Separate probes exceed application NVM,
physical DTCM and shared SRAM; each must fail at link time. Small images must
still link. The Arm GCC CI job runs these checks with its required toolchain.
Region checks for scatter files do not substitute for running the Arm linker.
