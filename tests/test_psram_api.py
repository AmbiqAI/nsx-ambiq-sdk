from __future__ import annotations

import subprocess
from pathlib import Path


def test_psram_common_behavior(repo_root: Path, tmp_path: Path) -> None:
    stub = tmp_path / "nsx_core.h"
    stub.write_text(
        """
#ifndef NSX_CORE_H
#define NSX_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define NSX_STATUS_SUCCESS 0u
#define NSX_STATUS_INVALID_HANDLE 1u
#define NSX_STATUS_INVALID_CONFIG 2u
#define NSX_STATUS_INVALID_VERSION 3u
typedef struct { uint32_t major, minor, revision; } nsx_version_t;
typedef struct { uint32_t apiId; nsx_version_t version; } nsx_core_api_t;
static inline bool nsx_core_check_api(
    const nsx_core_api_t *api, const nsx_core_api_t *oldest,
    const nsx_core_api_t *current) {
    (void)oldest;
    return api == NULL || api->apiId != current->apiId;
}
#endif
""",
        encoding="ascii",
    )
    harness = tmp_path / "test_psram.c"
    harness.write_text(
        """
#include <assert.h>
#include <string.h>
#include "nsx_psram_internal.h"

static uint32_t init_status;
static bool safe_to_retry;
static unsigned reads;
static unsigned writes;

uint32_t nsx_psram_platform_init(
    const nsx_psram_config_t *cfg, nsx_psram_platform_info_t *info) {
    info->base_address = cfg->enable_xip ? 0x60000000u : 0u;
    info->clock_hz = cfg->clock_hz;
    info->size_bytes = 16u;
    info->safe_to_retry = safe_to_retry;
    info->xip_enabled = cfg->enable_xip;
    info->timing_status = NSX_PSRAM_TIMING_VALID;
    info->rxdqs_delay = 14u;
    return init_status;
}
uint32_t nsx_psram_platform_read(
    uint32_t offset, void *buffer, uint32_t length) {
    (void)offset; (void)buffer; (void)length; ++reads;
    return NSX_STATUS_SUCCESS;
}
uint32_t nsx_psram_platform_write(
    uint32_t offset, const void *buffer, uint32_t length) {
    (void)offset; (void)buffer; (void)length; ++writes;
    return NSX_STATUS_SUCCESS;
}

int main(int argc, char **argv) {
    nsx_psram_config_t cfg;
    nsx_psram_info_t info;
    uint8_t data[4] = {0};
    const uint32_t capabilities =
        NSX_PSRAM_CAP_SYNC_TRANSFER | NSX_PSRAM_CAP_XIP |
        NSX_PSRAM_CAP_TIMING_SCAN;

    assert(nsx_psram_get_info(&info) == NSX_STATUS_SUCCESS);
    assert(info.state == NSX_PSRAM_STATE_UNINITIALIZED);
    assert(info.capabilities == capabilities);
    assert(nsx_psram_read(0, data, sizeof(data)) ==
           NSX_PSRAM_STATUS_NOT_INITIALIZED);
    assert(nsx_psram_default_config(&cfg) == NSX_STATUS_SUCCESS);
    if (argc == 2 && strcmp(argv[1], "xip-disabled") == 0) {
        cfg.enable_xip = false;
        assert(nsx_psram_init(&cfg) == NSX_STATUS_SUCCESS);
        assert(nsx_psram_get_info(&info) == NSX_STATUS_SUCCESS);
        assert(info.state == NSX_PSRAM_STATE_READY);
        assert(info.capabilities == capabilities);
        assert(!info.xip_enabled);
        assert(info.base_address == 0u);
        assert(nsx_psram_write(0u, data, sizeof(data)) ==
               NSX_STATUS_SUCCESS);
        assert(nsx_psram_read(0u, data, sizeof(data)) ==
               NSX_STATUS_SUCCESS);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "failed-state") == 0) {
        init_status = 99u;
        safe_to_retry = false;
        assert(nsx_psram_init(&cfg) == 99u);
        assert(nsx_psram_get_info(&info) == NSX_STATUS_SUCCESS);
        assert(info.capabilities == capabilities);
        assert(info.state == NSX_PSRAM_STATE_FAILED);
        assert(info.last_init_status == 99u);
        assert(nsx_psram_init(&cfg) == NSX_PSRAM_STATUS_FAILED_STATE);
        assert(nsx_psram_read(0u, data, 1u) ==
               NSX_PSRAM_STATUS_FAILED_STATE);
        return 0;
    }
    init_status = 99u;
    safe_to_retry = true;
    assert(nsx_psram_init(&cfg) == 99u);
    assert(nsx_psram_init(&cfg) == 99u);

    init_status = NSX_STATUS_SUCCESS;
    assert(nsx_psram_init(&cfg) == NSX_STATUS_SUCCESS);
    assert(nsx_psram_get_info(&info) == NSX_STATUS_SUCCESS);
    assert(info.state == NSX_PSRAM_STATE_READY);
    assert(info.capabilities == capabilities);
    assert(nsx_psram_read(16u, data, 1u) == NSX_PSRAM_STATUS_OUT_OF_RANGE);
    assert(nsx_psram_write(15u, data, 2u) == NSX_PSRAM_STATUS_OUT_OF_RANGE);
    assert(nsx_psram_read(0u, NULL, 0u) == NSX_STATUS_SUCCESS);
    assert(nsx_psram_write(0u, NULL, 0u) == NSX_STATUS_SUCCESS);
    assert(reads == 0u && writes == 0u);
    assert(nsx_psram_read(12u, data, sizeof(data)) == NSX_STATUS_SUCCESS);
    assert(nsx_psram_write(12u, data, sizeof(data)) == NSX_STATUS_SUCCESS);
    assert(reads == 1u && writes == 1u);
    return 0;
}
""",
        encoding="ascii",
    )

    include = repo_root / "modules/nsx-psram/includes-api"
    source = repo_root / "modules/nsx-psram/src/nsx_psram.c"
    internal = repo_root / "modules/nsx-psram/src"
    executable = tmp_path / "test_psram"
    subprocess.run(
        [
            "cc",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DNSX_PSRAM_PLATFORM_CAPABILITIES=7u",
            f"-I{tmp_path}",
            f"-I{include}",
            f"-I{internal}",
            str(source),
            str(harness),
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)
    subprocess.run([str(executable), "xip-disabled"], check=True)
    subprocess.run([str(executable), "failed-state"], check=True)
