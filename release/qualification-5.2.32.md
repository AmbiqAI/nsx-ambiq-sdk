# SDK 5.2.32 qualification

The local five-board GCC qualification passed 58 requirements. The baseline
was v5.2.31 (`d80399e91d28f78078198b75547bbe651a2e51bd`). The candidate
and fixed test harness were `decdc3a27824fdf99ae6994462b7b13cf6307e07`.
The two revisions have identical AmbiqSuite provider trees and all 73 archives.

| Coverage | Result |
| --- | --- |
| Host | GCC compatibility gate passed 1,996 tests. Full candidate suite passed 4,187 with 204 skips. The focused power contracts and links also passed all 31 selected cases. |
| GCC startup and runtime | 40 firmware runs, 100 Unity cases across AP3 Blue Plus, AP4 Blue Plus KXR, AP330 Plus, AP510 and AP510B. Each board ran startup, runtime, initialization ownership and full system initialization on both revisions. |
| New power APIs | 10 candidate firmware runs, 15 Unity cases on the same five boards. Each board ran the live IOM0 domain test and its target-specific buck test. AP3 exercised linked CTIMER pairs, AP4/AP510/AP510B exercised TIMER inputs, and AP330 verified the unsupported result. |
| Generated app | The released neuralSPOT-X 0.9.7 CLI built one app for AP510B and AP330 Plus with the SDK pinned to the candidate. Both boards reported IOM0 off/on/off. In 250 ms AP510B counted 11,955 / 20,407 / 0 / 9,549 pulses (VDDC / VDDF / VDDS / VDDC_LV). AP330 Plus returned unsupported. |
| Link coverage | GCC, ATfE and ACfE each linked and validated the minimal firmware layout on AP330 Plus, AP510, AP510B and AP510 Lite (12 links). |

Physical runs used GCC and debugger reset. Each board's original application
flash region was restored and read back. AP510 Lite has link coverage here.
Power-domain status and raw pulse counts have separate support. Per-rail energy
calibration, physical clock accuracy, cold power-on and BLE traffic were outside
this run.

From the retained evidence directory `artifacts/nsx/release532-mHGviZ`, run
`sha256sum --check "$SDK/release/evidence-5.2.32.sha256"` with `SDK` set
to this checkout. The ledger includes the host and board records, generated
app source and lock, and compiler link records.

## Cordio follow-up

With the Cordio update from AmbiqSuite `eff2a4850038` applied to SDK
`d04b0c8`, Arm GNU GCC built `libnsx_cordio.a` for Apollo3, Apollo3 Plus,
Apollo4 Plus Blue KXR and Apollo510B. The Apollo510B BLE smoke application
linked and ran on the EVB after a debugger reset. A Bluetooth central
discovered its service, read model `Apollo510B EVB` and firmware `5.2.32`,
received 10 periodic notifications, and verified the asynchronous payloads
`123456` and `654321` across a disconnect and reconnect.
The flashed ELF SHA-256 was
`79c2f9a1c4ad43e48792161421b88459af3c1edc664c605c29a64cabae219616`.
