# Shared data initialization

GNU startup copies bytes from `_init_data_sram` to `[_ssdata, _sedata)`.
The load image begins at `LOADADDR(.shared)`, so `_ssdata` must include the
resource-table prefix as well as initialized shared data. All three symbols
describe the same output-section window. Shared subsections belong to that
window too; `.sram_bss` remains separate and zero-initialized.

The matrix covers Apollo4L/P, Apollo510/B, legacy Apollo5B descriptors, AT110
and the Apollo330P/510L controls. Supported profiles keep their existing memory
apertures; the tests do not independently establish silicon memory capacities.

Empty copies must check the end before the first load/store, including when the
destination is the exclusive end of SRAM. The Apollo4L/P and Apollo510B/legacy
Apollo5B loops now follow that rule for their data/shared/ITCM copies.

Validation uses real GNU/ATfE links and, when available, QEMU execution of the
data/shared/ITCM-copy instructions extracted unchanged from production startup. QEMU uses
scratch addresses and direct entry; it does not model Apollo peripherals, the
full boot flow or physical memory apertures. Existing small-M55 startup tests
remain in place.
