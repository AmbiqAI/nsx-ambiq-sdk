# 5.2.28 qualification candidate

This patch release starts at `e67ba8cfe7c4045e65b0def5168d5ac44711ed55`
(SDK #187 merged). It includes #176, #178, #181, #182, #184, #186 and #187
since v5.2.27. The generated payload under `modules/nsx-ambiqsuite/sdk/`, including all 73
HAL/BSP archives, remains byte-identical. Provider wrapper metadata and README
change to reflect the distribution version.
Historical evidence and source/train exclusions remain in the 5.2.27 record;
they are not presented as fresh 5.2.28 hardware qualification.

## Candidate acceptance

Candidate `7e99645e15b1e6a6ff3a60ed78f775299f5acc22` passed:

- All local pre-commit hooks, including the full pytest suite.
- All three required CI jobs (Python 3.10/3.12 and ARM GCC integration),
  [run 41935922004](https://github.com/AmbiqAI/nsx-ambiq-sdk/actions/runs/41935922004).
- Release metadata/manifests/repository checks: 60 passed.
- Four-board real link smokes with GCC, ATfE and ArmClang: AP330 Plus,
  AP510, AP510B and AP510 Lite. This is link/ELF validation, not hardware execution.
- Normal neuralspotx generated AP4 KXR and AP510B apps with an explicit
  app-local candidate revision override: lock, frozen configure and frozen
  build. All seven SDK entries in each lock resolve to the candidate commit.
  These runs qualify candidate integration, not the not-yet-published tag.
- Independent release review: no blocking findings; 100 bounded tests passed.

Local Copilot identified two release-record wording issues: an overly broad
provider identity claim and a stale description of the 5.2.26 consumer path.
Both are corrected and cleared by independent and local Copilot follow-up reviews.
All three required CI jobs also passed on `a0382fd`.
The registry promotion will align project, module and SoC-family pins, regenerate
example receipts through `nsx lock`, and validate fresh apps without an override
after publication. No tag exists yet. Raw logs, firmware inputs and review
reports are retained outside the repository in the local SDK528 evidence bundle.

## Hardware scope

On clean candidate `a0382fdf167171064024a973e4e4331a4619d89d`, AP510B
(`apollo510b_evb`, probe 1160002954) passed nine Unity cases with GCC:

- Shared-memory initialization and placement: five volatile values covering both
  NSX/HAL include orders and the native attribute were poisoned to `0xDEADBEEF`
  and read back before reset-entry execution. All restored their distinct flash
  initializers and lay within the linked shared-data bounds (two cases).
- Initialized data, zeroed BSS and C++ constructor startup (three cases).
- Stack alignment/bounds, heap allocation, heap-limit rejection and SVC dispatch
  (four cases).

The local API acquired the shared lab claim and probe lease, programmed each
image, executed normal reset entry, and required fresh build identities with
zero failures/ignored cases. The claim was released; runtime test firmware
remains on the board. Manifests, ELF/input hashes, seed readback, transport and
Unity records are retained in the local SDK528 evidence bundle under
`hw-shared-startup`, `hw-startup` and `hw-runtime`.

This is debugger-connected GCC reset evidence, not cold boot, DMA/cache
coherence, sleep, BLE radio traffic or a broad board qualification. IAR coverage
remains preprocessing-only. The final release-record commit changes no tested
firmware inputs or production implementation.
