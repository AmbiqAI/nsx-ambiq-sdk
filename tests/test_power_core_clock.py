"""Execute the actual performance selector against a controllable HAL."""

from pathlib import Path
import json
import os
import shlex
import shutil
import subprocess

import pytest

from test_cmake_contract import configure_contract_project
from tools.nsx_npu_compile_check import find_gcc


@pytest.fixture(params=("apollo510", "apollo510b", "apollo330P", "apollo510L", "apollo510-fpga", "apollo330P-fpga", "apollo510L-fpga"))
def clock_harness(request, repo_root: Path, tmp_path: Path) -> Path:
    family = request.param
    (tmp_path / "nsx_power.h").write_text('''
#include <stdint.h>
#include "nsx_core.h"
typedef enum { NSX_POWER_PERF_NOT_SET=-1, NSX_POWER_PERF_LOW=0,
    NSX_POWER_PERF_HIGH=1, NSX_POWER_PERF_MAX=2 } nsx_power_perf_mode_t;
uint32_t nsx_power_set_performance_mode(nsx_power_perf_mode_t);
''')
    (tmp_path / "am_mcu_apollo.h").write_text('''
#include <stdint.h>
#include <stddef.h>
#define AM_HAL_STATUS_SUCCESS 0u
#define AM_HAL_STATUS_INVALID_ARG 1u
#define AM_HAL_STATUS_INVALID_OPERATION 7u
#if defined(APOLLO5_FPGA)
#define AM_HAL_CLKGEN_FREQ_MAX_HZ (APOLLO5_FPGA * 1000000u)
#define AM_HAL_CLKGEN_FREQ_HP250_HZ 20000000u
#else
#define AM_HAL_CLKGEN_FREQ_MAX_HZ 96000000u
#define AM_HAL_CLKGEN_FREQ_HP250_HZ 250000000u
#endif
extern uint32_t SystemCoreClock;
#define AM_HAL_STATUS_FAIL 4u
typedef struct {struct {unsigned MCUPERFACK,MCUPERFSTATUS;} MCUPERFREQ_b;} power_regs;
extern power_regs mock_power;
#define PWRCTRL (&mock_power)
#define CLKGEN_HF2ADJ0_HF2ADJEN_EN 1u
typedef struct { struct { volatile unsigned HF2ADJEN; } HF2ADJ0_b; } clock_regs;
extern clock_regs mock_clkgen;
#define CLKGEN (&mock_clkgen)
#if defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
#define HAS_HP2 1
#if defined(APOLLO5_FPGA)
#define AM_HAL_CLKGEN_FREQ_HP192_HZ 16000000u
#else
#define AM_HAL_CLKGEN_FREQ_HP192_HZ 192000000u
#endif
typedef enum { AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER=0,
    AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE1=1,
    AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE2=2 } am_hal_pwrctrl_mcu_mode_e;
#else
#define HAS_HP2 0
typedef enum { AM_HAL_PWRCTRL_MCU_MODE_LOW_POWER=0,
    AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE=1 } am_hal_pwrctrl_mcu_mode_e;
#endif
uint32_t am_hal_pwrctrl_mcu_mode_select(am_hal_pwrctrl_mcu_mode_e);
''')
    # Use the packaged HAL's configuration values, including the free-run
    # sentinel, rather than assuming every successful query returns hertz.
    clk_header = (repo_root / "modules/nsx-ambiqsuite/sdk/mcu/apollo510/hal/am_hal_clkmgr.h").read_text()
    hfrc2_definitions = "\n".join(line for line in clk_header.splitlines()
                                  if line.startswith("#define AM_HAL_CLKMGR_HFRC2_FREQ_"))
    (tmp_path / "am_hal_clkmgr.h").write_text(hfrc2_definitions + "\n" + '''
#define AM_HAL_CLKMGR_CLK_ID_PLLVCO 5
#define AM_HAL_CLKMGR_CLK_ID_HFRC2 6
uint32_t am_hal_clkmgr_clock_config_get(unsigned, uint32_t *, void *);
''')
    harness = tmp_path / "harness.c"
    harness.write_text('''
#include <assert.h>
#include <string.h>
#include "am_mcu_apollo.h"
#include "nsx_power.h"
#include "am_hal_clkmgr.h"
uint32_t SystemCoreClock = 777;
clock_regs mock_clkgen;
power_regs mock_power;
static unsigned select_status, query_status, pll_hz=221184000, hfrc2_hz=AM_HAL_CLKMGR_HFRC2_FREQ_FREE_RUN_APPROX_250MHZ;
static unsigned selected=999, selects, queries;
static unsigned hardware_fault;
uint32_t am_hal_pwrctrl_mcu_mode_select(am_hal_pwrctrl_mcu_mode_e mode) {
    assert(SystemCoreClock == 777); // Publish only after hardware succeeds.
    selected=mode; ++selects; mock_power.MCUPERFREQ_b.MCUPERFACK=hardware_fault!=1; mock_power.MCUPERFREQ_b.MCUPERFSTATUS=hardware_fault==2 ? 99u : (unsigned)mode; return select_status;
}
uint32_t am_hal_clkmgr_clock_config_get(unsigned id, uint32_t *hz, void *cfg) {
    assert(id == (HAS_HP2 ? 5u : 6u) && cfg == NULL);
    ++queries; *hz=HAS_HP2 ? pll_hz : hfrc2_hz;
    return query_status;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    const char *test=argv[1];
    nsx_power_perf_mode_t mode=NSX_POWER_PERF_MAX;
    if (!strcmp(test,"low")) mode=NSX_POWER_PERF_LOW;
    if (!strcmp(test,"not-set")) mode=NSX_POWER_PERF_NOT_SET;
    if (!strcmp(test,"high")) mode=NSX_POWER_PERF_HIGH;
    if (!strcmp(test,"audio-high")) mode=NSX_POWER_PERF_HIGH;
    if (!strcmp(test,"audio-high") || !strcmp(test,"audio-max")) hfrc2_hz=196608000, mock_clkgen.HF2ADJ0_b.HF2ADJEN=1;
    if (!strcmp(test,"pending-audio")) hfrc2_hz=196608000;
    if (!strcmp(test,"adjusted-250")) {
        hfrc2_hz=AM_HAL_CLKMGR_HFRC2_FREQ_ADJ_250MHZ;
        mock_clkgen.HF2ADJ0_b.HF2ADJEN=1;
    }
    if (!strcmp(test,"unsupported-source")) pll_hz=hfrc2_hz=200000000;
    if (!strcmp(test,"select-fails")) select_status=91;
    if (!strcmp(test,"query-fails")) query_status=92;
    if (!strcmp(test,"zero-source")) pll_hz=hfrc2_hz=0;
    if (!strcmp(test,"excessive-source")) pll_hz=hfrc2_hz=300000000;
    if (!strcmp(test,"low-source")) pll_hz=hfrc2_hz=96000000;
#if !HAS_HP2 && !defined(APOLLO5_FPGA)
    if (!strcmp(test,"audio-then-free") || !strcmp(test,"released-audio")) {
        hfrc2_hz=AM_HAL_CLKMGR_HFRC2_FREQ_ADJ_196P608MHZ;
        mock_clkgen.HF2ADJ0_b.HF2ADJEN=1;
        assert(nsx_power_set_performance_mode(mode)==AM_HAL_STATUS_INVALID_OPERATION && selects==0 && SystemCoreClock==777);
        SystemCoreClock=777; selects=queries=0;
        mock_clkgen.HF2ADJ0_b.HF2ADJEN=0;
        if (!strcmp(test,"audio-then-free")) hfrc2_hz=AM_HAL_CLKMGR_HFRC2_FREQ_FREE_RUN_APPROX_250MHZ;
    }
#endif
    if (!strcmp(test,"invalid")) {
        const int invalid[]={-2,3,99};
        for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i)
            assert(nsx_power_set_performance_mode((nsx_power_perf_mode_t)invalid[i])==NSX_STATUS_INVALID_CONFIG);
        assert(selects==0 && queries==0 && SystemCoreClock==777); return 0;
    }
    if (!strcmp(test,"missing-ack") || !strcmp(test,"wrong-hardware")) {
        hardware_fault=!strcmp(test,"missing-ack") ? 1 : 2;
        for (unsigned i=0;i<2;++i) {
            assert(nsx_power_set_performance_mode(mode)==AM_HAL_STATUS_FAIL);
            assert(SystemCoreClock==777);
        }
        hardware_fault=selects=queries=0;
    }
    unsigned result=nsx_power_set_performance_mode(mode);
#if defined(APOLLO5_FPGA)
    unsigned bad_source=0;
#else
    unsigned bad_source=(!HAS_HP2 && mock_clkgen.HF2ADJ0_b.HF2ADJEN && hfrc2_hz==196608000) || query_status || (HAS_HP2 ? pll_hz < 192000000 || pll_hz > 250000000 : (hfrc2_hz != AM_HAL_CLKMGR_HFRC2_FREQ_FREE_RUN_APPROX_250MHZ && hfrc2_hz != AM_HAL_CLKMGR_HFRC2_FREQ_ADJ_250MHZ && hfrc2_hz != AM_HAL_CLKMGR_HFRC2_FREQ_ADJ_196P608MHZ));
#endif
    if (bad_source) {
        assert(result != 0 && selects == 0 && SystemCoreClock == 777);
        if (query_status) assert(result == query_status);
    } else if (select_status) {
        assert(result == select_status && selects == 1 && SystemCoreClock == 777);
    } else {
        assert(result == 0 && selects == 1);
        unsigned expected_mode=mode==NSX_POWER_PERF_HIGH ? 1 : mode==NSX_POWER_PERF_MAX ? (HAS_HP2 ? 2 : 1) : 0;
#if defined(APOLLO5_FPGA)
        unsigned expected_hz=expected_mode==0 ? AM_HAL_CLKGEN_FREQ_MAX_HZ : AM_HAL_CLKGEN_FREQ_HP250_HZ;
#if HAS_HP2
        if (expected_mode==1) expected_hz=AM_HAL_CLKGEN_FREQ_HP192_HZ;
#endif
        assert(queries == 0);
#else
        unsigned expected_hz=expected_mode==0 ? 96000000 : HAS_HP2 ? (expected_mode==1 ? 192000000 : pll_hz) : !mock_clkgen.HF2ADJ0_b.HF2ADJEN || hfrc2_hz==AM_HAL_CLKMGR_HFRC2_FREQ_FREE_RUN_APPROX_250MHZ ? 250000000 : hfrc2_hz;
        assert(queries == (expected_mode!=0 && (!HAS_HP2 || expected_mode==2) ? 1u : 0u));
#endif
        assert(selected == expected_mode && SystemCoreClock == expected_hz);
    }
}
''')
    cc = shutil.which("cc")
    assert cc is not None, "A host C compiler is required"
    target = tmp_path / "harness"
    command = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", f"-DAM_PART_{family.split('-')[0].upper()}", f"-I{tmp_path}",
               "-I" + str(repo_root / "modules/nsx-core/includes-api"),
               "-I" + str(repo_root / "modules/nsx-core/includes-internal"),
               str(repo_root / "modules/nsx-core/src/nsx_performance.c"),
               str(repo_root / "modules/nsx-power/src/nsx_power_performance.c"), str(harness), "-o", str(target)]
    if family == "apollo510b":
        command.append("-DAM_PART_APOLLO510")
    if family.endswith("-fpga"):
        command.append("-DAPOLLO5_FPGA=8")
    # GCC_ROOT selects the Arm compiler for NSX tests, but host GCC also
    # interprets it as its own installation prefix. Do not redirect host
    # builtin headers into an Arm toolchain installation in cross-compile CI.
    host_env = os.environ.copy()
    host_env.pop("GCC_ROOT", None)
    result = subprocess.run(command, capture_output=True, text=True, env=host_env)
    assert result.returncode == 0, result.stderr
    return target


@pytest.mark.parametrize("scenario", ("low", "high", "max", "not-set", "select-fails", "query-fails", "zero-source", "excessive-source", "low-source", "audio-high", "audio-max", "adjusted-250", "unsupported-source", "audio-then-free", "pending-audio", "released-audio", "invalid", "missing-ack", "wrong-hardware"))
def test_performance_updates_core_clock(clock_harness: Path, scenario: str) -> None:
    result = subprocess.run([str(clock_harness), scenario], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize("board", ("apollo510_evb", "apollo510b_evb", "apollo330mP_evb", "apollo510dL_evb"))
@pytest.mark.parametrize("family", ("gcc", "atfe", "armclang"))
def test_performance_compiles_with_real_hal(repo_root: Path, tmp_path: Path, board: str, family: str) -> None:
    if family == "gcc":
        compiler = find_gcc(None)
        if compiler is None and os.environ.get("NSX_REQUIRE_ARM_GCC") == "1":
            pytest.fail("Arm GCC is required")
    elif family == "atfe":
        compiler = Path(os.environ.get("ATFE_ROOT", "/opt/toolchains/atfe")) / "bin/clang"
        if not compiler.is_file():
            if "ATFE_ROOT" in os.environ:
                pytest.fail("ATFE_ROOT does not contain bin/clang")
            compiler = None
    else:
        found = shutil.which("armclang")
        compiler = Path(found) if found else None
    if compiler is None:
        pytest.skip(f"{family} is unavailable")
    result = configure_contract_project(repo_root, tmp_path, board, family,
        ("nsx-core", "nsx-power"), prelude=("set(CMAKE_EXPORT_COMPILE_COMMANDS ON)",))
    assert result.returncode == 0, result.stdout
    build = tmp_path / family / board / "build"
    entries = json.loads((build / "compile_commands.json").read_text())
    selected = [entry for entry in entries if entry["file"].endswith(("/nsx_power_performance.c", "/nsx_performance.c"))]
    assert len(selected) == 2, "Compile the adapter and core-owned selector exactly once each"
    for entry in selected:
        command = shlex.split(entry["command"])
        command[0] = str(compiler)
        command[command.index("-o") + 1] = str(tmp_path / (Path(entry["file"]).stem + ".o"))
        result = subprocess.run(command, cwd=entry["directory"], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize("board", ("apollo4p_blue_kxr_evb", "apollo330mP_evb", "apollo510_evb", "apollo510b_evb"))
def test_performance_smoke_builds(repo_root, tmp_path, board):
    from test_small_m55_memory import compiler_for_test
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import PERFORMANCE
    compiler = compiler_for_test('gcc')
    build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, PERFORMANCE))
