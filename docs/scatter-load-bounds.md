# Aggregate scatter load bounds

An execution-region limit bounds that region's runtime contents. It does not
bound the complete load image, which includes initialized RAM contents stored
in application NVM. Every staged MCU_MRAM scatter profile therefore limits
`LR_1` to the same application aperture as its MCU_MRAM region.

The fix preserves existing application origins and sizes. It adds the missing
aggregate limit to Apollo4L/P, Apollo510/B, legacy Apollo5B descriptors and AT110;
Apollo330P/510L retain their existing bounds. This is not a requalification or
change of each family's physical capacity.

`test_scatter_load_bounds.py` uses real armlink, retains allocations with
`--no_remove`, checks ELF physical load ends and proves both exact-boundary
acceptance and combined RO/initialized-RAM overflow rejection. Run it with
Arm GCC and armlink on PATH before an ArmClang-qualified release. A skipped
linker case is not qualification evidence.
