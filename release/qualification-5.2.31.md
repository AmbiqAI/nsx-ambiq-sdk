# SDK 5.2.31 qualification

On November 19, 2026, the local five-board GCC qualification passed all 58
requirements for candidate and harness `8b505aa2a6c6905235065fe7abfbd8337219acc2`
against v5.2.30 (`acc97030b7158f81dc11f8df68fc43b8346f6dfe`).
Both revisions use the same AmbiqSuite provider, including all 73 archives.

| Scope | Result |
| --- | --- |
| Host | GCC compatibility gate 1,993 passed. Full host suite 4,174 passed, 200 skipped. Repository hygiene passed. |
| AP3 Blue Plus, AP4 Blue Plus KXR, AP330 Plus, AP510, AP510B | Four suites on each revision and board. All 100 Unity case executions passed across 40 firmware runs. The checks exercise C/C++ startup, heap and stack behavior, SVC dispatch, initialization ownership and full system initialization. |
| AP510 PSRAM on the candidate | Six firmware runs, 48 Unity cases passed. Three common cases, five blocking cases and ten async cases for each combination of 96/250 MHz CPU and requested 48/96 MHz PSRAM. Coverage includes per-client completion, queue depth, XIP during DMA and blocking/async interaction. |

All hardware checks used debugger reset. AP510B timed out observing the
v5.2.30 startup result on the first attempt and the v5.2.30 startup-ownership
result on the second. Reconnecting after the second timeout recovered the
matching build ID and both passing cases from SRAM. The third complete run
passed with the original probe settings and 40-second timeout. The cause is open.

Physical SCK, throughput ([#214](https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/214)),
cold power-on and ATfE/ACfE execution were outside this run. The host and board
records are retained under `artifacts/nsx/sdk207-release-IlEYlQ` in the parent
workspace. The release manifest pins SHA-256 digests for the assessment, host
and board records, both timed-out attempts, and the six PSRAM build manifests
and results. Verify a retained file with `sha256sum` from that evidence root.
