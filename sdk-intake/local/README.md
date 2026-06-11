# Local SDK Intake Workspace

This directory is the **single ignored workspace** for raw AmbiqSuite SDK drops
and extracted build trees. Nothing here except this `README.md` is tracked.

```text
sdk-intake/local/drops/   # raw AmbiqSuite_*.zip drops
sdk-intake/local/work/    # extracted / building SDK source
```

Place SDK zips under `drops/` and run `sdk-intake/build_ambiqsuite.py`; the
helper extracts into `work/` and copies approved, source-controlled artifacts
into `artifacts/ambiqsuite/<version>/...`. See `../README.md` for the full
intake workflow.
