# 5.2.28 qualification candidate

This patch release starts at `e67ba8cfe7c4045e65b0def5168d5ac44711ed55`
(SDK #187 merged). It includes #176, #178, #181, #182, #184, #186 and #187
since v5.2.27. All provider files and 73 HAL/BSP archives remain byte-identical.
Historical evidence and source/train exclusions remain in the 5.2.27 record;
they are not presented as fresh 5.2.28 hardware qualification.

## Candidate acceptance

Version/provenance integrity, full local checks, available link smokes,
independent and local Copilot reviews, required CI, and neuralspotx generated-app
build qualification are pending. Exact results will be recorded in release
PR #188 before publication. No tag exists yet.

## Hardware scope

Proposed bounded AP510B debugger-reset test: poison shared-memory sentinel
values before reset-entry execution, verify both legacy include orders and
native shared attributes initialize from flash, then run existing startup
smokes. Flash authorization/board reservation and execution are pending.
No cold boot, DMA/cache coherence, sleep, BLE radio traffic or new broad board
qualification is claimed. IAR coverage remains preprocessing-only.
