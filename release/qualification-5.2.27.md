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

The implementation candidate `f690dcd38c32024ac122148884edf3a9ea0c8847`
passed all 1,916 required GCC compatibility cases (58 other-profile
deselections). A freshly isolated pipx installation of the neuralspotx 0.9.0
wheel passed `nsx doctor`, created the normal default AP4 KXR app, resolved the
candidate through an app-local registry override, and passed frozen
configure/build. App sources were unchanged; no SDK-root override was used.
All vendored files were verified against the candidate's Git blob identities.

With probe `1160001491` claimed, frozen flash and a seven-second SWO capture
passed, producing seven `nsx hello from generated app` messages. The claim was
released. ELF SHA-256:
`59ed7b5815ec4ba9f3c6e357eef3fb59770764db2dcfa4aaa5b3b0e87bba52f0`.
NSX wheel SHA-256:
`6b6d862dd09d0f7e196f3c056ea8a56778768e46f0cbee9d42e4be671972beeb`.

Subsequent release-record-only corrections do not change that implementation.
The final reviewed candidate must repeat frozen vendoring/build and physical
flash/SWO acceptance; its exact SHA and hashes are recorded in PR #124 before
merge approval. This avoids putting a self-referential final commit in this
file. Final review and CI are separate gates, not implied by these runs.

Evidence is retained outside the repository at
`artifacts/nsx/release-124-refresh-20261028`, including original attempts,
source/lock identities, manifests, ELF, raw logs and reviewer reports. After
publication, update the packaged registry to the immutable tag and repeat
generated-app acceptance against that tag.

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
