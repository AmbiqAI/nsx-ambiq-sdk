"""Exercise the firmware teardown with real Unity assertion aborts on the host."""
import os
import shutil
import subprocess


def test_clock_cleanup_restores_masks_even_when_unity_aborts(repo_root, tmp_path):
    firmware = (repo_root / 'tests/hardware/clock/main.c').read_text()
    teardown = firmware[firmware.index('void tearDown(void)'):firmware.index('static void log_sample')]
    source = tmp_path / 'cleanup.c'
    source.write_text(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "unity.h"
static struct { uint32_t CTRL; } systick;
static struct { uint32_t ICSR; } scb;
static struct { struct { uint32_t MCUPERFSTATUS; } MCUPERFREQ_b; } power;
#define SysTick (&systick)
#define SCB (&scb)
#define PWRCTRL (&power)
#define SCB_ICSR_PENDSTCLR_Msk 1
#define NSX_POWER_PERF_LOW 0
static bool initialized;
static uint32_t saved_basepri=7, saved_primask=1, basepri, primask;
static uint32_t SystemCoreClock, result, calls;
static void __set_BASEPRI(uint32_t value) { basepri=value; }
static void __set_PRIMASK(uint32_t value) { primask=value; }
static uint32_t nsx_power_set_performance_mode(int mode) {
    assert(mode==NSX_POWER_PERF_LOW); ++calls; return result;
}
void setUp(void) {}
static void body(void) {}
''' + teardown + r'''
int main(void) {
    for (unsigned fault=0; fault<5; ++fault) {
        initialized=fault!=4;
        result=fault==1 ? 1 : 0;
        SystemCoreClock=fault==2 ? 250000000 : 96000000;
        power.MCUPERFREQ_b.MCUPERFSTATUS=fault==3 ? 2 : 1;
        basepri=primask=calls=0;
        systick.CTRL=7; scb.ICSR=0;
        UnityBegin("clock cleanup");
        RUN_TEST(body);
        assert(Unity.TestFailures == (fault>=1 && fault<=3));
        assert(basepri==saved_basepri && primask==saved_primask);
        assert(systick.CTRL==0 && scb.ICSR==SCB_ICSR_PENDSTCLR_Msk);
        assert(calls==(unsigned)initialized);
    }
    return 0;
}
''')
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required'
    unity = repo_root / 'tests/hardware/vendor/unity'
    binary = tmp_path / 'cleanup'
    env = os.environ.copy()
    env.pop('GCC_ROOT', None)
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(unity), str(source), str(unity/'unity.c'), '-o', str(binary)],
                   check=True, capture_output=True, text=True, env=env, timeout=30)
    subprocess.run([str(binary)], check=True, capture_output=True, text=True, timeout=10)
