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

## Verification of the boundary

A no-network regression test clones a local candidate, poisons its test CMake
file, and successfully builds using the harness tests. It then breaks a
production header in that candidate and requires the build to fail at that
header. Other tests check build-before-lease sequencing, dirty/changed-checkout
rejection, preserved existing output, and fail-closed aggregate reporting.
