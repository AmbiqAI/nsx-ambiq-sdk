"""Exercise the real common PSRAM implementation with controlled IRQ delivery."""
from __future__ import annotations

import subprocess
from pathlib import Path

import pytest


def _compile(repo: Path, binary: Path, depth: int = 8, *, source: Path | None = None,
             unsupported: bool = False) -> None:
    module = repo / 'modules/nsx-psram'
    combined = binary.with_suffix('.harness.c')
    combined.write_text(
        f'#include "{source or module / "src/nsx_psram.c"}"\n'
        f'#include "{repo / "tests/fixtures/psram-async/main.c"}"\n'
    )
    subprocess.run([
        'cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
        '-DNSX_PSRAM_TEST_POINTS', f'-DNSX_PSRAM_ASYNC_DEPTH={depth}',
        f'-DNSX_PSRAM_PLATFORM_CAPABILITIES={7 if unsupported else 15}u',
        *(['-DWITHOUT_ASYNC', '-Wno-unused-function', '-Wno-unused-parameter'] if unsupported else []),
        f'-I{repo / "tests/fixtures/psram-async"}',
        f'-I{module / "includes-api"}', f'-I{module / "src"}',
        str(combined), '-o', str(binary),
    ], check=True)


@pytest.fixture(scope='module', params=[1, 8, 32])
def async_binary(repo_root: Path, tmp_path_factory: pytest.TempPathFactory, request) -> Path:
    binary = tmp_path_factory.mktemp(f'psram-async-{request.param}') / 'test'
    _compile(repo_root, binary, request.param)
    return binary


@pytest.mark.parametrize('scenario', [
    'checks', 'depth', 'clients', 'errors', 'blocking', 'blocking-error',
    'wrap', 'early-completion',
])
def test_async_contract(async_binary: Path, scenario: str) -> None:
    subprocess.run([str(async_binary), scenario], check=True, timeout=10)


def test_async_interleavings(async_binary: Path) -> None:
    # Enumerate every external test-point position plus points beyond the script.
    point, observed = 0, 1
    while point <= observed:
        result = subprocess.run([str(async_binary), 'interleave', str(point)],
                                check=True, capture_output=True, text=True, timeout=10)
        observed = max(observed, int(result.stdout))
        point += 1


def test_async_unsupported(repo_root: Path, tmp_path: Path) -> None:
    binary = tmp_path / 'unsupported'
    _compile(repo_root, binary, unsupported=True)
    subprocess.run([str(binary), 'checks'], check=True, timeout=10)


def test_async_model_rejects_mutants(repo_root: Path, tmp_path: Path) -> None:
    # Mutate real implementation rather than maintaining a second state machine.
    source = (repo_root / 'modules/nsx-psram/src/nsx_psram.c').read_text()
    mutants = [
        ("return pending;", "return pending + 1u;", 'clients'),
        ("if (nsx_psram_client_pending(client) == 0u)", "if (nsx_psram_pending() == 0u)", 'clients'),
        ("if (nsx_psram_client_pending(client) == 0u)", "if (true)", 'clients'),
        ("++client->issued;", "/* missing publication */", 'depth'),
        ("slot->queued = false;\n    ++client->completed;", "/* leaked slot */\n    ++client->completed;", 'depth'),
        ("status = nsx_psram_platform_read_async(offset, buffer, length, slot, previously_pending);",
         "nsx_psram_platform_irq_restore(irq);\n"
         "status = nsx_psram_platform_read_async(offset, buffer, length, slot, previously_pending);\n"
         "irq = nsx_psram_platform_irq_save();", 'depth'),
    ]
    for index, (before, after, case) in enumerate(mutants):
        assert before in source
        changed = tmp_path / f'mutant{index}.c'
        changed.write_text(source.replace(before, after))
        binary = tmp_path / f'mutant{index}'
        _compile(repo_root, binary, source=changed)
        result = subprocess.run([str(binary), case], capture_output=True, timeout=10)
        assert result.returncode != 0, (index, result.stdout, result.stderr)
