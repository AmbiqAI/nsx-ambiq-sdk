#!/usr/bin/env bash
set -euo pipefail

workspace="${1:-/workspace}"

cd "${workspace}"

mkdir -p "$(dirname "${UV_PROJECT_ENVIRONMENT:?UV_PROJECT_ENVIRONMENT must be set}")"
git config --global --add safe.directory "${workspace}"

if [[ -n "${ARMLM_ACTIVATION_CODE:-}" ]]; then
  echo "ARMLM_ACTIVATION_CODE is set; activating Arm license for root."
  "${ACFE_ROOT:?ACFE_ROOT must be set}/bin/armlm" activate -code "${ARMLM_ACTIVATION_CODE}"
fi

if [[ -z "${ARMLM_ACTIVATION_CODE:-}" ]]; then
	echo "No Arm license env is set; armclang license checks may fail."
else
	echo "Using activated Arm license cache for armclang."
fi

uv sync --group ci
