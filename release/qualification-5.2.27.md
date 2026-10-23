# 5.2.27 qualification candidate

Prepared 2026-10-23. This is an NSX integration release over the unchanged
5.2.26 AmbiqSuite provider. All 73 HAL/BSP archives, headers and the artifact
manifest remain unchanged. Historical provenance and limitations are retained
in the 5.2.26 report and the new release manifest.

## Evidence and scope

The implementation baseline is SDK commit
`4e7a78e10d419d2b067160de7368b1f3a7752f04` (merged PRs #116–#122).
The release commit additionally aligns versions and records qualification.

- Local regression at the implementation baseline: 1,572 passed, 184 skipped.
- Clean-checkout GCC SDK compatibility gate: 288 passed, 40 other-toolchain
  cases deselected.
- Seven Unity startup/runtime cases passed on each of AP3 Blue Plus,
  AP4 Blue Plus KXR, AP330 Plus, AP510 and AP510B. Runs used GCC and an attached
  debugger. The suite covers startup state, constructors, memory, stack/heap
  and exception dispatch; it is not peripheral qualification.
- SVC negative controls on AP3 Blue Plus and AP510B failed the expected
  assertion; normal firmware was restored and passed.
- AP4 BSP selection was checked with UART diagnostics. KXR emitted messages
  in two runs; KBR initialized but emitted none on the connected console.
  The selected board is `apollo4p_blue_kxr_evb`; this does not prove every
  peripheral pin mapping.
- AP330 passed with J-Link 9.50 and the verified per-session device pack.
  Pack binaries are not redistributed in this public repository.

Local raw core evidence is retained at
`/home/adamp/Ambiq/neuralspotx/artifacts/nsx/sdk-core-qualification-2026-10-23`.
These machine-local artifacts are not shipped as part of the release.

## Generated-app validation

Built the neuralspotx 0.9.0 wheel from commit
`2b72ffc71f90df792825645ac9c9647f296ca019`, installed it in isolated pipx,
and ran `nsx doctor` successfully. Created the normal default application for
`apollo4p_blue_kxr_evb`, then selected the merged SDK through an explicit
app-local registry override:

```yaml
module_registry:
  projects:
    nsx-ambiq-sdk:
      revision: 4e7a78e10d419d2b067160de7368b1f3a7752f04
```

Normal configure vendored that revision and recorded it in `nsx.lock`.
No SDK-root override or application source edit was used. The frozen build
passed. With the AP4 probe claimed, the following operations succeeded:

```sh
nsx flash --app-dir "$APP" --frozen --probe-serial 1160001491 --timeout 90
nsx view --app-dir "$APP" --frozen --probe-serial 1160001491 \
  --duration 7 --capture hello-swo.log --timeout 30
```

SWO captured seven consecutive `nsx hello from generated app` messages in
seven seconds. Supply the serial on each operation: a later configure did not
retain the serial supplied to the initial configure, and an attempt without
an explicit serial failed to connect in this multi-probe environment.

ELF SHA-256:
`29c8bef3e6ff8258fd7615ce6faaad3f6cb3c2f1a9916d5624e40ed0da57b636`.
Logs, lock, manifest, unchanged source and binaries are retained locally at
`/home/adamp/Ambiq/neuralspotx/artifacts/nsx/generated-app-2026-10-23`.
This validates the merged SDK integration baseline, not a published 5.2.27 tag.

## Remaining qualification limits

- AP3 physical microphone capture remains unresolved. Passing core tests does
  not resolve microphone power, routing or acoustic input.
- Adjusted-HFRC2 audio remains unresolved (#72). The experimental upstream
  clock-selector proposal is not included. Earlier AP510B HP250/PLL audio
  results do not qualify the adjusted-HFRC2 path.
- The five-board results do not establish standalone cold boot, low-power
  transitions, long-duration DMA, or general peripheral correctness.
- No new ATfE or ArmClang hardware qualification was performed for this
  candidate. Earlier toolchain evidence remains historical, not a new pass.
- Descriptor-only and experimental targets retain their existing exclusions.
- Publishing and the packaged neuralspotx registry pin are separate steps.
  After approval, publish the immutable tag, update the registry and repeat
  the generated-app check against the published release.
