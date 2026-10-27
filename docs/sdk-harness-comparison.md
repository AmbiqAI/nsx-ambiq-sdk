# Compare two SDKs with one fixed harness

Run this from a clean, committed test-harness checkout. Tests and the Python
runner come from this checkout; the two explicit SDK roots supply production
modules, board descriptors, CMake, startup/linker inputs and HAL/BSP archives.
No code under either candidate's `tests/` or `tools/` is loaded. Production CMake
from each SDK is necessarily executed, so use SDK sources you intend to build.

```sh
uv sync --group ci --group hardware
uv run python -m tools.hardware.compare \
  --baseline-sdk-root /path/to/baseline-sdk \
  --candidate-sdk-root /path/to/candidate-sdk \
  --board apollo510b_evb --suite runtime \
  --gcc-root /usr/local/arm-gnu-toolchain \
  --build-only --output /tmp/nsx-sdk-comparison
```

All three roots must be clean committed repositories. The harness root is the
checkout containing the executing comparison module, not the current working
directory or a sibling guess. The API accepts a `ComparisonRequest` containing
the two SDK roots and the same explicit board/compiler/output selections.

For hardware, acquire the shared lab claim first, replace `--build-only` with
`--hardware --probe SERIAL`, and use a fresh output directory. Both firmware
images build before the runner acquires one exclusive local probe lease for the
two sequential executions. The two descriptors must agree on the SEGGER device.
Release the shared lab claim afterward. Each firmware suite is responsible for
restoring its own board baseline. A failure stops the comparison; inspect its
raw evidence and restore the board before further use.

`--suite clock --clock-experimental` selects the provisional clock/SysTick
suite. It is not an SDK qualification profile. The fitted crystal and timing
error-budget prerequisites in [the clock contract](clock-timebase-qualification.md)
still apply. See [clock lab tests](clock-lab-tests.md) for the negative controls.

## Evidence and verdicts

The fresh output contains `baseline/`, `candidate/` and `comparison.json`.
Each build manifest (schema 3) records separate `harness` and `sdk` roots,
revisions and dirty flags, firmware/compiler identity and resolved input hashes.
Hashes cover source and dependency headers, CMake input files, startup/linker
files, archives, harness Python tools, Unity and the reporter. Input roles
separate harness, SDK, generated build files and environment/toolchain files.
Schema 3 replaces the old flat SDK revision/dirty fields with these two records.

The comparison checks each build manifest against the initial clean checkout
identities before acquiring a probe, and rechecks all checkouts after execution.
Failure, interruption, a changing checkout, or incomplete evidence cannot leave
a successful aggregate. An existing nonempty output is refused. A build-only
result says `built`, never hardware `passed`. Clock runs say `experimental_pass`;
all comparison reports have `qualification: false`. Compare the two captured
runs and manifests; this command does not infer release approval or a performance
regression percentage from dissimilar binaries.

## Single-SDK API and pytest

`BuildRequest.repo` is the harness root. Its optional `sdk_root` selects a
production checkout; omitting it preserves the original same-checkout behavior.
The equivalent pytest option is `--candidate-sdk-root /path/to/sdk`. For example:

```sh
uv run pytest -q tests/hardware/test_runtime.py --build-only \
  --board apollo510b_evb --gcc-root /usr/local/arm-gnu-toolchain \
  --candidate-sdk-root /path/to/sdk --hardware-artifacts /tmp/nsx-candidate
```

Single builds allow dirty working trees and record them for development. The
comparison command deliberately requires clean commits for reproducibility.
A startup omission control transforms the selected SDK's startup source into
an artifact-only copy using the fixed harness's mutation logic. Its supported
board remains AP3 Blue Plus; no production checkout is modified.

## SDK-intake acceptance checklist

1. Stage and commit the candidate using [the intake workflow](sdk-drop-workflow.md).
   Select clean, immutable baseline SDK, candidate SDK and reviewed harness
   revisions. SDK roots are **NSX SDK module repositories**, not raw AmbiqSuite
   trees. Preserve release history/tags for host checks.
2. Complete the [snapshot and patch review](../sdk-intake/README.md#snapshot-and-patch-review).
   Record every patch disposition against those exact revisions. Review changed
   headers, archives, build inputs and NSX adapters; the file inventory is not a
   semantic API/ABI verdict. For new functionality, decide whether NSX exposes
   it, retains direct HAL access, or intentionally leaves it unsupported.
3. Declare the board/toolchain matrix and extra tests before running them. The
   GCC core baseline is `startup` (three Unity cases) and `runtime` (four) on
   `apollo3p_evb`, `apollo4p_blue_kxr_evb`, `apollo330mP_evb`, `apollo510_evb`
   and `apollo510b_evb`. Select additional existing shim tests and bounded board
   smokes from the changed interfaces and prior defects. Missing required boards
   remain incomplete coverage; other compilers/variants need explicit coverage.
4. Run the candidate host gate and fixed-harness comparisons below. Review
   changes to candidate-owned tests separately. Use relevant failure controls
   when validating a changed test boundary; failures must be the intended
   assertions, not timeouts or transport errors, and cleanup must succeed.
5. Review the complete evidence and exclusions before accepting the intake.
   This qualifies NSX integration, not the full HAL or every peripheral. A
   genuinely new SDK must pass this process; comparing snapshots with identical
   production inputs only rehearses the tooling. Merge/release approval remains
   a separate maintainer decision.

### Local commands

Set absolute paths for this intake. Use a new evidence directory outside all
three checkouts. The harness must include the comparison command; install its
[host prerequisites and device pack](local-hardware-tests.md#environment) first.
No board-farm runner or Helia installation is required.

```sh
set -eu
HARNESS=/path/to/reviewed-harness
BASELINE=/path/to/approved-nsx-sdk
CANDIDATE=/path/to/candidate-nsx-sdk
EVIDENCE=/path/to/new-intake-evidence
GCC=/path/to/arm-gnu-toolchain
DECISIONS=/path/to/reviewed-patch-decisions.json
mkdir -p "$EVIDENCE"
unset PYTEST_ADDOPTS GCC_ROOT
export PATH="$GCC/bin:$PATH"

# Verify the completed patch review still matches these exact snapshots.
cd "$HARNESS"
uv sync --locked --group ci --group hardware
uv run --group ci python -m tools.sdk_intake_review \
  --baseline "$BASELINE" --candidate "$CANDIDATE" --decisions "$DECISIONS" \
  --output "$EVIDENCE/intake-reviewed.json"

# Candidate-owned host gate; all required cases must pass without skips.
cd "$CANDIDATE"
uv sync --locked --group ci
uv run --group ci pre-commit run --all-files
uv run --group ci python -m tools.sdk_compatibility \
  --profile gcc --output "$EVIDENCE/candidate-gcc"

# Fixed test definitions for every baseline/candidate hardware pair.
cd "$HARNESS"
uv sync --locked --group ci --group hardware
BOARD=apollo510b_evb
PROBE=YOUR_RESERVED_PROBE_SERIAL
SUITES="startup runtime"
for SUITE in $SUITES; do
  uv run --group ci --group hardware python -m tools.hardware.compare \
    --baseline-sdk-root "$BASELINE" --candidate-sdk-root "$CANDIDATE" \
    --board "$BOARD" --probe "$PROBE" --gcc-root "$GCC" \
    --suite "$SUITE" --hardware --hardware-timeout 40 \
    --output "$EVIDENCE/$BOARD/$SUITE" || exit 1
done
```

Run the loop in a POSIX shell (for example Bash). Reserve the exact board before
starting it, keep J-Link sessions sequential, and release the claim afterward.
Repeat for each declared board with its correct descriptor/probe. Stop on the
first failed command; investigate and restore the board before retrying into
fresh output.
For a hardware-free rehearsal, replace `--hardware --probe "$PROBE"` with
`--build-only`; that can never complete hardware acceptance.

Clock testing is an additional regression screen when the intake affects that
boundary, not a required oscillator-characterization campaign. The existing
`clock` suite supports AP330 Plus/AP510B with `--clock-experimental`; if selected,
run its [negative controls and cleanup](clock-lab-tests.md#negative-controls-and-cleanup)
against the candidate using the fixed harness, then a normal run. Preserve its
[reference assumptions](clock-timebase-qualification.md); an experimental pass
is not calibrated frequency qualification.

### Required evidence and decision

Keep the complete output directories, not only console PASS lines:

- `intake-reviewed.json` must say `review-recorded` with exit zero. This records
  patch decisions, not test success or automatic provenance/qualification.
- `candidate-gcc/compatibility.json` must say `passed`, with process exit zero;
  retain JUnit, exact cases, exclusions, compiler identity and build artifacts.
- Each core-suite `comparison.json` must say `passed`, with both runs passed.
  Require all seven startup/runtime cases for each declared board and SDK.
- If clock testing was selected, its comparison must say `experimental_pass`;
  retain all 72 windows per SDK/board and the negative-control/restoration
  evidence. This is a
  regression screen, not calibrated frequency qualification.
- Preserve manifests, ELF/input hashes, raw results, Unity logs, execution and
  J-Link/device-pack metadata. Record board revision, probe, reset/debug policy,
  lab conditions (including whether temperature was measured or assumed),
  harness/baseline/candidate commits and reviewed SDK/patch changes.

Every comparison deliberately retains `qualification: false`: separate suite
results do not automatically grant release approval. The intake reviewer checks
matrix completeness, unchanged clean identities, expected failure controls and
remaining exclusions. Failed/incomplete/skipped required work prevents acceptance;
a build-only result or an old successful run cannot replace missing evidence.
The clock accuracy assumption, AP3 microphone work, sleep/cold boot, RTOS and
peripheral coverage remain explicit gaps rather than implied passes.
