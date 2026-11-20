#ifndef NSX_POWER_STATUS_H
#define NSX_POWER_STATUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NSX_POWER_STATUS_OK = 0,
    NSX_POWER_STATUS_INVALID,
    NSX_POWER_STATUS_UNSUPPORTED,
    NSX_POWER_STATUS_HAL_ERROR
} nsx_power_status_result_t;

enum {
    NSX_POWER_STATUS_VALID_DEVICE      = 1u << 0,
    NSX_POWER_STATUS_VALID_MCU_MEMORY  = 1u << 1,
    NSX_POWER_STATUS_VALID_SHARED_SRAM = 1u << 2,
    NSX_POWER_STATUS_VALID_SYSTEM      = 1u << 3,
    NSX_POWER_STATUS_VALID_AUDIO       = 1u << 4,
    NSX_POWER_STATUS_VALID_REGULATOR   = 1u << 5,
    NSX_POWER_STATUS_VALID_SUPPLY      = 1u << 6
};

/** Raw status words. Decode with the selected SoC's CMSIS register masks. */
typedef struct {
    uint32_t valid;       /**< Mask of observed words. Unavailable words are zero. */
    uint32_t device;      /**< DEVPWRSTATUS. Apollo3 IOM0/1/2 share the HCPB domain. */
    uint32_t mcu_memory;  /**< MEMPWRSTATUS. */
    uint32_t shared_sram; /**< SSRAMPWRST. */
    uint32_t system;      /**< SYSPWRSTATUS, including core power. */
    uint32_t audio;       /**< AUDSSPWRSTATUS. */
    uint32_t regulator;   /**< VRSTATUS. */
    uint32_t supply;      /**< Apollo3 SUPPLYSTATUS with separate main and BLE buck bits. */
} nsx_power_status_t;

/**
 * Read live power domains into caller-owned storage without changing power policy.
 * The words are sampled sequentially. Hardware may transition between reads.
 * No initialization or buck session is required. A null output returns INVALID.
 * On any failure caller storage is untouched.
 */
nsx_power_status_result_t nsx_power_status_read(nsx_power_status_t *out);

#ifdef __cplusplus
}
#endif

#endif
