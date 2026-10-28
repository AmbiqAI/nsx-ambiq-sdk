# 5.2.27 qualification candidate

Refreshed 2026-10-28. This is an NSX integration release over the unchanged
5.2.26 AmbiqSuite provider, not a new upstream SDK intake. All 73 HAL/BSP
archives, headers and the artifact manifest remain unchanged. Historical
provenance and exclusions remain in the 5.2.26 report and release manifest.
Release, tag and publication require separate maintainer approval.

## Implementation and evidence

Implementation baseline: `23636da044960ed626d92b68632ced88109a4db2` (main after
#131 and #174). The release branch adds version alignment and release records.
Exact candidate revisions, commands, hashes and final review/CI results are
recorded in [release PR #124](https://github.com/AmbiqAI/nsx-ambiq-sdk/pull/124).

The current-main GCC compatibility gate passed 1,916 cases with 58 explicit
other-profile deselections. The five-board matrix ran production baseline
`fc5e344576e22500f4fa53706dcb184f504d2581`; subsequent #131 changes documentation,
and #174 corrects an AP4 test assertion without changing production behavior.

| Physical board | Core Unity cases | Additional passing Unity cases |
| --- | ---: | ---: |
| AP3 Blue Plus | 7 | 9 |
| AP4 Blue Plus KXR | 7 | 17 + 1 corrected debug case |
| AP330 Plus | 7 | 15 |
| AP510 | 7 | 12 |
| AP510B | 7 | 14 |

Core cases cover initialized data/BSS/constructors, stack/heap bounds and SVC
exception dispatch. Selected extras cover startup ownership/status, performance
transitions, debug lifecycle, timer periods and applicable power/memory/USB
preservation/TempCo policies. These are debugger-connected GCC reset tests.
AP4's original debug assertion failed because it read clock-gated ITM state;
#174 checks final trace/clock gates. The corrected case passed at
`f1bcb24a1ac08185b21648fa6c801b46470da894`; omitting disable failed the intended
assertion, then the normal passing image was restored. All claims were released.

Raw evidence and `acceptance-summary.json` are retained outside the repository
under `artifacts/nsx/acceptance-2026-10-28` in the parent workspace. Original
failures and negative controls are preserved, not counted as passing cases.
Prior AP510 USB traffic and audio evidence remains linked from its implementation
PRs; this matrix did not repeat those traffic/acoustic experiments.

## Release-candidate acceptance

Candidate checks and generated-app acceptance are being refreshed in PR #124.
Do not interpret the implementation baseline matrix as an exact-candidate pass.
The required app flow uses the installed neuralspotx 0.9.0 wheel, an unmodified
AP4 KXR default app, an explicit immutable candidate-SHA registry override,
normal app-local vendoring, frozen configure/build, and explicit-probe flash/SWO
capture. No SDK-root override is permitted for this acceptance.

Candidate evidence is retained outside the repository at
`artifacts/nsx/release-124-refresh-20261028`. Final acceptance must record the
SDK candidate and NSX wheel hashes, lock, ELF hash, commands, raw output and
reviewed exclusions before release approval. After publication, update the
packaged registry to the immutable tag and repeat generated-app acceptance.

## Limits

- AP3 microphone work remains paused. Adjusted-HFRC2 audio is unqualified;
  the withdrawn upstream clock-selector proposal is not included.
- No standalone cold-power-cycle/PVT, broad sleep/RTOS/peripheral, interrupt
  latency, physical AP4 USB teardown or cable-unplug qualification is claimed.
- AP510L has compile/link and controlled shim coverage, not physical validation.
  AP4 constrained-memory layouts remain intentionally rejected pending proof.
- ATfE/ArmClang have inherited and per-PR evidence; no fresh hardware
  qualification is claimed for those toolchains in this release preparation.
- Experimental AT110 retains header/archive and timer support limitations;
  descriptor-only/experimental targets are outside the physical matrix.
- Vendor-internal completion waits remain deferred. The shim's returned errors
  do not promise hardware rollback or safe continued execution after a partial
  startup failure.
