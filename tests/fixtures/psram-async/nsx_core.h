#ifndef TEST_NSX_CORE_H
#define TEST_NSX_CORE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define NSX_STATUS_SUCCESS 0u
#define NSX_STATUS_INVALID_HANDLE 1u
#define NSX_STATUS_INVALID_CONFIG 3u
#define NSX_STATUS_INVALID_VERSION 2u
typedef struct { uint32_t major, minor, revision; } nsx_version_t;
typedef struct { uint32_t apiId; nsx_version_t version; } nsx_core_api_t;
static inline bool nsx_core_check_api(const nsx_core_api_t *api,
    const nsx_core_api_t *oldest, const nsx_core_api_t *current) {
    return !api || api->apiId != current->apiId ||
        api->version.major != oldest->version.major ||
        api->version.minor < oldest->version.minor ||
        api->version.minor > current->version.minor;
}
#endif
