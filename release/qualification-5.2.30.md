# 5.2.30 qualification

This release packages #199 and #201's vendor heap-stub correction. The tested
candidate is `fa61571630bb0c7ebebb703fbca67b86334b5386`; its provider tree is
identical to merged #201 (`1e88777afebabbb8c918b9ef215f2563bb3edc42`).
Upstream remains `3cfb7d50718e6fb016f1a0128121cb40598109d5`; cumulative
prepared source is `83bd69c37edf8d94cf77021140d707c3fe886242`.

All 73 native archives were rebuilt (GCC 25, ATfE 25, ACfE 23). Relative to
5.2.29, 54 archives change bytes and 19 remain identical. Independent object
inspection found changes only in 18 HAL global objects: six parts across three
toolchains. Every other HAL and BSP object is byte-identical; other archive
changes are metadata. Retained code/data and non-debug relocation semantics
are unchanged. The invalid weak `_sbrk` and associated sections are removed.

The 961 nonarchive provider files retain 5.2.29 paths, modes and bytes, excluding
the updated artifact manifest. The existing XSPI and staged driver/header fixes
remain. Full raw source regeneration encountered the separate sanitation gap
tracked in #200; this is an explicitly verified archive-only promotion.

## Evidence

On 2026-11-07 (America/Chicago), GCC startup and runtime suites passed on
AP330 Plus, AP510B and AP4 Blue Plus KXR: six firmware runs, 21 Unity cases,
zero failures or ignored cases. Coverage includes initialized data, zeroed BSS,
C++ constructors, allocation/write/read/free, heap/stack bounds and alignment,
oversized `_sbrk` rejection with ENOMEM and unchanged break, and SVC dispatch.
The initial AP330 attempt lacked the optional hardware Python dependency;
after installing the declared group, all runs passed.

The candidate compatibility gate passed 1,989 tests. Full local pre-commit,
two independent reviews, local Copilot, hosted Copilot and CI passed without
blocking findings. Four GCC/ATfE-newlib C++ constructor link cases retain the
immutable 5.2.29 negative control; an archive-wide test rejects returned stubs.
The link fixture allocates before main; the hardware constructor does not.

[PR #201 final assessment](https://github.com/AmbiqAI/nsx-ambiq-sdk/pull/201#issuecomment-6715522674)
records evidence reconciliation. Raw build, firmware and execution records are
retained outside the repo under `artifacts/nsx/sbrk42-20261107/` in the parent
workspace. Hosted Copilot could not inspect those local records; independent
source/object checks and retained runtime results cover that limitation.

## Release delta and limits

Release preparation updates coordinated versions and BLE default firmware
strings, plus records and tests. The provider tree must remain identical to
the tested candidate. Release checks are recorded in the release PR.
NSX's bounded allocator and force-link protection remain unchanged.

These are debugger-controlled GCC runs, not cold-power-on qualification.
No fresh USB/BLE traffic, performance, sleep/resume, RF/PVT, current-draw,
worst-case stack, microphone, or ATfE/ACfE hardware claims are made. AP510L
was not physically tested; AT110 remains experimental. Earlier release
hardware evidence is historical, not inherited qualification for this refresh.
Publication and immutable tag creation are separate approved steps.
