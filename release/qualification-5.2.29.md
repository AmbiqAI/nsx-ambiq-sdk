# 5.2.29 qualification

This release packages the provider intake and EM9305 fixes through SDK
`3648b7c254ec711bef98747b204c023685b06b1d` (#189, #191, #192, #194, #196,
#197). The provider snapshot is `stable-2026.11.05`: upstream
`3cfb7d50718e6fb016f1a0128121cb40598109d5`, with AT110 XSPI source patches
producing prepared revision `6a2a0c08af553e33716f2e251cb74aa370e2ae0f`.
All 73 HAL/BSP archives were rebuilt; the release manifest records their
provenance separately from the four staged-payload patches and Cordio base.

## Integration evidence

On 2026-11-07, clean candidate `3648b7c254ec711bef98747b204c023685b06b1d`
was compared against 5.2.28 (`e528eba6f43a45b97e08072ab46340f1ac3fdb16`)
using the latter's unchanged hardware harness and GCC 15.2.1.

| Physical board | Paired suites |
| --- | --- |
| AP3 Blue Plus | startup, runtime, AP3 performance transitions |
| AP4 Blue Plus KXR | startup, runtime, timer period, performance transitions |
| AP330 Plus | startup, runtime, power startup, timer period, performance transitions |
| AP510 | startup, runtime, power startup, timer period, performance transitions |
| AP510B | startup, runtime, power startup, timer period, performance transitions |

All 22 comparison pairs passed: 44 firmware runs and 148 Unity case executions,
including the seven core startup/runtime cases on every board and SDK.

Additional frozen-fixture traffic checks passed on both SDK versions:

- AP510 USB: 2,000 outbound frames and 1,870 inbound telemetry frames per run;
  raw CRC, payload and sequence validation passed, with zero device errors.
  The historical host counter snapshot reports 1,854 before its reader joins;
  independent decoding covers the final in-flight block too.
- AP510B BLE: discovery, control/data exchange, unsubscribe behavior and reconnect.
  Sampled candidate radio-task unused stack was 3,654 words (14,616 bytes)
  out of 4,096 words; this is ordinary-smoke evidence, not a worst-case bound.

The candidate GCC compatibility gate passed 1,987 tests and full pre-commit
passed. An exact-revision patch review and 126 focused tests passed; restoring
the old 16-byte EM9305 receive scratch reproduced an ASan overflow with a
17-byte transfer. This host negative control does not imply fresh hardware
fault-injection coverage for every board suite.

Two evidence audits checked the raw results, firmware/input hashes and scope.
Evidence and review details are recorded in the
[qualification ledger](https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/132#issuecomment-6707675522).
Raw artifacts remain outside the repository in
`artifacts/nsx/intake-qualification-20261107/` in the parent workspace.
Individual comparison reports intentionally retain `qualification: false`;
release acceptance is a separate judgement over the declared matrix.

## Release delta and limits

Release preparation changes coordinated versions, release records and tests.
The only compiled production changes after the tested candidate are BLE
Device Information Service default firmware strings changing to `5.2.29`.
Provider files and all archives must remain byte-identical to that candidate.
The focused release BLE check and final review/CI status are recorded in the
release PR; preparing this record does not publish a tag or GitHub release.

These are debugger-reset GCC tests. They do not establish cold-power-on,
sleep/resume, RF/PVT, sustained throughput, current draw or worst-case stack
behavior. Performance suites exercise mode transitions, not throughput parity;
power suites exercise initialization and policy. AP510L was not physically
exercised, AP3 microphone work remains deferred, and AT110 remains experimental.
ATfE/ACfE built artifacts are distinct from GCC physical qualification; older
release hardware evidence does not qualify the new provider. Cordio retains
its separately recorded base and reviewed NSX transport changes.

The frozen review tool labels the prepared source as upstream; its retained
supplement explicitly records both identities. The qualification audit checked
committed prepared-source objects and patch hashes, not a fresh full archive
rebuild. The intake PR contains the original rebuild and staging evidence.
