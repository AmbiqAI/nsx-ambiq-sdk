"""Execute target-usable clock arithmetic against bounded reference captures."""
from pathlib import Path
import os
import shutil
import subprocess


def test_clock_capture_arithmetic(repo_root: Path, tmp_path: Path):
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required'
    source = tmp_path / 'measurement.c'
    source.write_text(r'''
#include <assert.h>
#include <stddef.h>
#include "measurement.h"
int main(void) {
    nsx_clock_sample_t sample = {0};
    /* 1/4 s at 250 MHz with both 32-bit counters crossing zero. */
    assert(nsx_clock_sample(0xFF000000u, 45722784u, 0xFFFFF000u, 4096u,
                           8192u, 270000000u, 32766u, &sample));
    assert(sample.cpu_cycles == 62500000u && sample.reference_ticks == 8192u);
    assert(nsx_clock_rate_within(sample, 32768u, 250000000u, 250000000u));
    assert(!nsx_clock_rate_within(sample, 32768u, 96000000u, 96000000u));
    assert(!nsx_clock_rate_within(sample, 16384u, 249000000u, 251000000u));
    /* 96 MHz, no wrap; exact inclusive bounds and sub-Hz boundary rejection. */
    assert(nsx_clock_sample(100u, 24000100u, 100u, 8292u,
                           8192u, 270000000u, 32766u, &sample));
    assert(nsx_clock_rate_within(sample, 32768u, 96000000u, 96000000u));
    sample = (nsx_clock_sample_t){3u, 2u};
    assert(!nsx_clock_rate_within(sample, 1u, 1u, 1u));
    assert(nsx_clock_rate_within(sample, 1u, 1u, 2u));
    /* Reference/cycle stalls, excessive/ambiguous windows and impossible rate. */
    assert(!nsx_clock_sample(0u, 100u, 1u, 1u, 8192u, 270000000u, 32766u, &sample));
    assert(!nsx_clock_sample(1u, 1u, 0u, 100u, 8192u, 270000000u, 32766u, &sample));
    assert(!nsx_clock_sample(0u, 100u, 0u, 8193u, 8192u, 270000000u, 32766u, &sample));
    assert(!nsx_clock_sample(0u, 100u, 0u, 100u, UINT32_MAX, 270000000u, 32766u, &sample));
    assert(!nsx_clock_sample(0u, 1000000u, 0u, 1u, 8192u, 270000000u, 32766u, &sample));
    assert(!nsx_clock_sample(0u, 1u, 0u, 1u, 0u, 1u, 1u, &sample));
    assert(!nsx_clock_sample(0u, 1u, 0u, 1u, 1u, 0u, 1u, &sample));
    assert(!nsx_clock_sample(0u, 1u, 0u, 1u, 1u, 1u, 0u, &sample));
    assert(!nsx_clock_sample(0u, 1u, 0u, 1u, 1u, 1u, 1u, NULL));
    assert(!nsx_clock_rate_within((nsx_clock_sample_t){0u, 1u}, 1u, 1u, 2u));
    assert(!nsx_clock_rate_within((nsx_clock_sample_t){1u, 0u}, 1u, 1u, 2u));
    assert(!nsx_clock_rate_within((nsx_clock_sample_t){1u, 1u}, 0u, 1u, 2u));
    assert(!nsx_clock_rate_within((nsx_clock_sample_t){1u, 1u}, 1u, 2u, 1u));
    /* Maximum-width products must not overflow before promotion. */
    sample = (nsx_clock_sample_t){UINT32_MAX, UINT32_MAX};
    assert(nsx_clock_rate_within(sample, UINT32_MAX, UINT32_MAX, UINT32_MAX));
    assert(nsx_clock_rate_within(sample, UINT32_MAX, UINT32_MAX - 1u, UINT32_MAX));
    return 0;
}
''')
    binary = tmp_path / 'measurement'
    env = os.environ.copy()
    env.pop('GCC_ROOT', None)  # Cross-toolchain roots must not affect host headers.
    result = subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                             '-fsanitize=undefined', '-fno-sanitize-recover=all',
                             '-I' + str(repo_root / 'tests/hardware/clock'), str(source),
                             '-o', str(binary)], env=env, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(binary)], env=env, capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr
