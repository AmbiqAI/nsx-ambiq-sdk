# SDK 5.2.33 qualification

Candidate `bddbe4e0b1a6e4d39722cf0cfa1714e997e8514b` retains the 5.2.32 provider, including all
73 HAL/BSP archives. The product change is SDK #247, which selects the
HAL and BSP archives when a CMake build directory is reused.

| Check | Result |
| --- | --- |
| Host CMake and vendor ABI contracts | 109 passed |
| Promoted provider | Verified the manifest hashes of all 73 archives. |
| Link and ELF layout checks | All 12 passed across Apollo330 Plus, Apollo510, Apollo510B and Apollo510 Lite with GCC, ATfE and ArmClang. |
| Generated Apollo510 hello app, NSX 1.0.1 and GCC | Reconfigured and built the same directory with the frozen vendor, override A, override B, then frozen vendor again. Each link command selected both archives from the expected root. All four builds preserved the lock and produced the same binary. |
| Apollo510 EVB | After debugger reset the board emitted 5 hello messages over SWO. Restored and verified the original 128 KiB flash region. |

Firmware SHA-256: `c838f13bbb225b69a6d9114b839e200726665cb5108b693876cc166512f7c996`.
Physical coverage was one board, one compiler and debugger reset.
The console repeatedly printed `nsx hello from generated app`.
From the external evidence directory, run
`sha256sum -c /path/to/nsx-ambiq-sdk/release/evidence-5.2.33.sha256`
to verify the retained records, link commands and tested binary.
The [manifest](nsx-ambiq-sdk-5.2.33.yaml) records the tested Git trees and the
[evidence ledger](evidence-5.2.33.sha256) identifies the saved build and bench records.
