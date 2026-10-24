#ifndef TEST_NSX_CORE_H
#define TEST_NSX_CORE_H
#include <stdint.h>
#define NSX_STATUS_INVALID_HANDLE 1u
#define NSX_STATUS_INVALID_VERSION 2u
typedef struct { unsigned major, minor, revision; } nsx_version_t;
typedef struct { uint32_t apiId; nsx_version_t version; } nsx_core_api_t;
static inline uint32_t nsx_core_check_api(const nsx_core_api_t *a, const nsx_core_api_t *b, const nsx_core_api_t *c) { (void)a; (void)b; (void)c; return 0; }
#endif
