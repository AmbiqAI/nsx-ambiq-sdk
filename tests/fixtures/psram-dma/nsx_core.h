#ifndef TEST_NSX_CORE_H
#define TEST_NSX_CORE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define NSX_STATUS_SUCCESS 0u
typedef struct { uint32_t major, minor, revision; } nsx_version_t;
typedef struct { uint32_t apiId; nsx_version_t version; } nsx_core_api_t;
#endif
