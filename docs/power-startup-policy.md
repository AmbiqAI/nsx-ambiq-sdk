# AP3/AP4 startup policy proposal

**Status: proposed, not enforced.** PR1 of [#137](https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/137)
changes documentation only. Accepting this design does not qualify the hardware
or make the existing API safe for runtime profile switching.

Scope: AP3/3P and AP4P/L, including their packaged board variants. AP2,
AP330/AP510 and other families retain their current behavior. Do not generalize
their clock, debug or power sequences from this proposal.

## Evidence and current behavior

The baseline is NSX `523fbfd4ea86db6fca5bba0debecc46b033c4865` and the intake's
AmbiqSuite source revision `3062036c4f3e7822d27e6a842f226f423328a25a`.
The [field trace](https://github.com/AmbiqAI/nsx-ambiq-sdk/issues/137#issuecomment-6404021799)
covers ten BSP variants; 42 packaged HAL/BSP archive hashes match the manifest.
These are source/provenance checks, not measurements of physical behavior.

Both [system initialization](../modules/nsx-core/src/nsx_system.c) and the
[AP3](../modules/nsx-power/src/apollo3/nsx_power.c)/[AP4](../modules/nsx-power/src/apollo4/nsx_power.c)
power adapters invoke BSP initialization. AP4 BSP/HAL initialization restores
memory defaults; Blue BSPs also assert external BLE reset. BSP initialization
disables RTC operation. Its `void` interface hides underlying operation failures.

| Field | AP3/3P today | AP4P today | AP4L difference |
| --- | --- | --- | --- |
| `api` | Version validation when enabled | Same | Same |
| `perf_mode` | LOW: normal; HIGH/MAX: burst | LOW: LP; HIGH/MAX: HP | Same adapter |
| `need_audadc` | Ignored | False disables AUDADC; true skips disable | Ignored |
| `need_ssram` | Ignored | BSP enables/retains all; false disables SSRAM and DSP0 memory; true keeps defaults and configures DAXI | DSP0 operation omitted |
| `need_crypto` | Ignored | False requests crypto shutdown; true skips it | Same |
| `need_ble` | Ignored; HAL has independent BLE initialization effects | Blue BSP asserts reset regardless; false adds reset writes; non-Blue has no reset-pin branch | Same distinction for Blue |
| `need_usb` | Ignored | USB/PHY disabled regardless | Ignored |
| `need_iom` | Ignored | False disables IOM0–7; true skips; IOS always disabled | Same |
| `need_uart` | Ignored | False disables UART0/3; UART1/2 always disabled | Same |
| `small_tcm` | Ignored | True requests 128 KiB DTCM only if SSRAM is requested; otherwise BSP's 384 KiB default remains | Same |
| `need_tempco` | True rejected | True starts helper; false does not stop an existing session | True rejected |
| `need_itm` | Ignored | False disables DEBUG domain; true skips, without initializing ITM | Same |
| `need_xtal` | Ignored | False applies 32 kHz XTAL deep-sleep power-down control; true does not undo it; RTC source selected to LFRC regardless | Same |
| `spotmgr_collapse` | Ignored | Ignored | Ignored |

The [presets](../modules/nsx-power/src/nsx_power.c) are not full hardware state
descriptions: `all_on` requests neither crypto nor TempCo, and AP4 still shuts
down other domains. On AP3/3P, `all_on` and `audio` have the same effective
configuration fields. The `small_tcm` implementation does not explicitly
configure the separate ITCM size claimed by its header comment.

## Proposed contract

1. **Cold startup only.** Apply a power policy once, from foreground, before
   affected drivers, DMA or interrupt-driven application activity begin. This
   does not add a core-initialized prerequisite to `nsx_power_configure()`.
   C runtime initialization has already occurred; its live memory must remain
   accessible. Dedicated performance, sleep and driver APIs own runtime changes.
2. **One coordinated owner.** System-only startup remains supported. A startup
   path selecting a power policy must not run a second BSP baseline afterward.
   Calling both complete initialization APIs sequentially is not a supported
   recipe. Do not implement this by silently skipping parts of the second call.
3. **Resource requirements, not driver initialization.** For supported peripheral
   fields, true excludes a resource from optional NSX shutdown after the mandatory
   baseline. It does not power/configure a driver or preserve a live device through
   cold-start resets. False permits the documented shutdown action, not a promise
   of zero current. Memory and TempCo have their own explicit effects.
4. **Preserve vendor sequencing.** BLE held in reset by a Blue BSP is permitted
   cold-start behavior, not by itself a defect. The driver owns boot/release and
   transport setup. Do not bypass BSP or blindly raise reset. USB true should omit
   optional USB/PHY shutdown; the driver still owns clock/PHY/stack activation.
5. **Family support is explicit.** Retain common configuration/preset compatibility
   by documenting unmanaged fields as no-ops and testing both values. Do not claim
   that BSP implements ignored flags. Unsupported TempCo and invalid performance
   modes continue to fail before effects. Preset descriptions must match actual
   supported policies, not promise that everything is enabled.
6. **Memory is constrained by the binary.** Before reducing memory, validate the
   supported linker layout, stack, data, allocator bounds and other live buffers.
   Make `small_tcm` and SSRAM independent only where that layout permits it. The
   normal AP4P linker allows heap growth across 384 KiB TCM; moving the early return
   does not make a 128 KiB configuration safe. Reject an unproven reduction before
   effects or require an explicitly supported constrained-memory build.
7. **Failure is not rollback.** Preflight errors are correctable and retryable.
   Once hardware changes begin, a reported failure requires reset unless a specific
   recovery path is separately verified. A second successful-policy application,
   including an identical request, must fail before hardware effects. Private
   TempCo cleanup/retry is not full-profile recovery. False TempCo means “do not
   start” here, not “stop an active session.”

## Coordination design and review gates

Prefer a small internal coordinator in the existing `nsx::system` layer, shared
by the full system and power startup paths. Keep family-specific hardware
operations in their backends. Power can depend on the shared boundary; core must
not depend on power or duplicate its field translation. The existing
`nsx_core_initialized()` boolean records runtime initialization, not BSP readiness,
and cannot serve as the coordinator.

Model unclaimed, in-progress, completed and failed startup explicitly. Reserve
ownership before hardware effects, reject re-entry, and publish the terminal
state afterward. Short state transitions must preserve the caller's interrupt
mask; do not mask interrupts across long vendor initialization/waits. Completion
means the selected NSX startup transaction completed, not that drivers are running.

Before PR2 implementation is accepted, resolve these details against callers:

- Define the supported staged recipe and treatment of `nsx_hw_init()`,
  `nsx_minimal_hw_init()` and `skip_bsp_init`. Skipping BSP must not be mistaken
  for having run it. Preserve valid system-only flows and document any deliberate
  compatibility change; do not infer external initialization from register guesses.
- Specify which NSX entry points share the guard and its status codes. Raw HAL/BSP
  calls cannot be intercepted; applications using them own sequencing and must not
  mix them with the coordinated recipe without an explicit contract.
- Determine whether debug setup occurs inside the startup transaction or after
  policy application for each supported recipe. Later steps must not undo selected
  resources. AP3 core `nsx_set_perf_mode()` is currently a no-op, whereas the power
  API controls burst mode: do not substitute one for the other. Preserve existing
  Apollo5 debug-before-performance ordering outside this scope.
- Define required observable postconditions and the limit of success reporting
  around the opaque BSP call. Checked NSX operations alone cannot establish that
  every hidden BSP operation succeeded. Do not recreate the HAL to remove this limit.
- Specify the linker/allocator evidence required before reducing memory, including
  other toolchains. Do not guess bounds or assume absence of linked SSRAM sections
  proves absence of live runtime buffers.

## Implementation wave and acceptance

| PR | Deliverable | Required evidence |
| --- | --- | --- |
| 1 | This proposal and source map | Review current versus proposed behavior and resolve design objections; no runtime qualification claim |
| 2 | Startup ownership and failure boundary | Actual-entry-point host tests for system-only and supported staged paths, preflight, repeats, re-entry and failures; real SDK links; focused AP3/AP4 startup smoke |
| 3 | Peripheral field/preset semantics | Independent field and meaningful combination tests, real headers/links, board checks for changed USB/BLE/debug effects; preserve vendor driver sequencing |
| 4 | Safe memory policy | Four TCM/SSRAM combinations, rejection of unsafe/unknown layouts before effects, supported linker/heap checks and bounded board memory tests |

PRs 3 and 4 depend on the agreed startup boundary; stack only actual dependencies.
Start physical validation on AP3 Blue Plus and AP4 Blue KXR. Compile coverage of
other variants is not a substitute for physical checks where their behavior differs.
Do not claim RF, USB protocol, analog accuracy or whole-HAL qualification from
these smoke tests. Keep evidence in workspace artifacts and summarize it in PRs;
this document remains the single design proposal rather than a running test log.

Merge implementation PRs only after relevant checks and reviews pass, with
explicitly recorded limitations. BSP error propagation that needs broader vendor
changes and a future runtime-profile API remain separate work.
