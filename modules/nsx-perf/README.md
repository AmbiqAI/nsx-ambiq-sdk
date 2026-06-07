# nsx-perf

`nsx-perf` provides generic performance capture primitives for NSX bare-metal
apps.

Contents:
- DWT cycle/CPI/exception/sleep/LSU/fold counters
- cache snapshot and delta helpers where supported
- Apollo5/Apollo330 PC sampling and ITM/SWO helpers

The API is generic across supported NSX bare-metal targets. Capabilities are
selected from the hardware counter support exposed by each SoC.
