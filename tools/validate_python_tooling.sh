#!/usr/bin/env bash
# Validate actual tooling and fresh pre-commit-style environments; never weaken TLS.
set -euo pipefail
cd "$(dirname "$0")/.."
validation_root="$(mktemp -d)"
trap 'rm -rf "$validation_root"' EXIT
uv --version
python3 --version
if command -v dpkg-query >/dev/null; then
    dpkg-query -W ca-certificates
fi
python3 - <<'PY'
import ssl
subjects = [str(cert["subject"]) for cert in ssl.create_default_context().get_ca_certs()]
assert not any("emSign ECC Root CA - G3" in subject for subject in subjects), "affected system root remains trusted"
print("System trust: affected emSign ECC Root CA - G3 absent")
PY
# Use private environments/caches so retained hook seeds cannot mask the result.
export UV_PROJECT_ENVIRONMENT="$validation_root/project"
export UV_CACHE_DIR="$validation_root/uv-cache"
export VIRTUALENV_OVERRIDE_APP_DATA="$validation_root/virtualenv"
uv sync --locked --group ci
uv run --no-sync python - <<'PY'
from importlib.metadata import version
import subprocess

def check(label, actual, minimum):
    print(f"{label}: {actual}", flush=True)
    assert tuple(map(int, actual.split('.'))) >= minimum, (label, actual, minimum)

check("uv", subprocess.check_output(["uv", "--version"], text=True).split()[1], (0, 13, 6))
check("virtualenv", version("virtualenv"), (21, 14, 12))
PY
uv run --no-sync python -m virtualenv --no-download --no-periodic-update "$validation_root/hook"
"$validation_root/hook/bin/python" - <<'PY'
import pip
from pip._vendor import certifi
print(f"fresh hook pip: {pip.__version__}")
print(f"fresh hook bundled certifi: {certifi.__version__}")
assert tuple(map(int, pip.__version__.split('.'))) >= (26, 2, 2)
assert tuple(map(int, certifi.__version__.split('.'))) >= (2026, 10, 19)
PY
# Exercise the fresh pip with TLS verification enabled and its own bundled trust.
"$validation_root/hook/bin/python" -m pip --isolated download --no-deps --no-cache-dir \
    --index-url https://pypi.org/simple --dest "$validation_root/wheels" PyYAML==6.0.3
PYTHONPYCACHEPREFIX="$validation_root/pycache" uv run --no-sync python -m py_compile sdk-intake/build_ambiqsuite.py
