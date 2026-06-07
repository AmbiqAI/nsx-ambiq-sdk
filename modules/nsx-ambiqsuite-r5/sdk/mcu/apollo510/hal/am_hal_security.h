//*****************************************************************************
//
//! @file am_hal_security.h
//!
//! @brief Functions for on-chip security features
//!
//! @addtogroup security_ap510 Security Functionality
//! @ingroup apollo510_hal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef AM_HAL_SECURITY_H
#define AM_HAL_SECURITY_H

#define AM_HAL_SECURITY_SOCID_NUMWORDS  8

//
// DCU controls needed for debugger
//
#define AM_HAL_DCU_DEBUGGER     (AM_HAL_DCU_SWD | AM_HAL_DCU_CPUDBG_INVASIVE | AM_HAL_DCU_CPUDBG_NON_INVASIVE | AM_HAL_DCU_CPUDBG_S_INVASIVE | AM_HAL_DCU_CPUDBG_S_NON_INVASIVE)
#define AM_HAL_DCU_SWO          (AM_HAL_DCU_CPUTRC_DWT_SWO | AM_HAL_DCU_CPUDBG_NON_INVASIVE | AM_HAL_DCU_CPUDBG_S_NON_INVASIVE)

//
//! Security Device LCS
//
typedef enum
{
    AM_HAL_SECURITY_LCS_CM = 0,
    AM_HAL_SECURITY_LCS_DM = 1,
    AM_HAL_SECURITY_LCS_SECURE = 5,
    AM_HAL_SECURITY_LCS_RMA = 7,
    AM_HAL_SECURITY_LCS_UNDEFINED = 0xff
} am_hal_security_device_lcs_e;

//
//! Security Info
//
typedef struct
{
    bool                            bInfo0Valid;
    am_hal_security_device_lcs_e    lcs;
    uint32_t                        sbrVersion;
    uint32_t                        info0Version;
    uint32_t                        sblVersion;
    uint32_t                        sblVersionAddInfo;
} am_hal_security_info_t;

//
//! Security Lock Type Definitions
//
typedef enum
{
    AM_HAL_SECURITY_LOCKTYPE_CUSTOTP_PROG   = 0x1,
    AM_HAL_SECURITY_LOCKTYPE_CUSTOTP_READ   = 0x2,
// #### INTERNAL BEGIN ####
    AM_HAL_SECURITY_LOCKTYPE_AMBOTP_PROG    = 0x11,
    AM_HAL_SECURITY_LOCKTYPE_AMBOTP_READ    = 0x12,
    AM_HAL_SECURITY_LOCKTYPE_INFO1ACCESS    = 0x9E,
// #### INTERNAL END ####
} am_hal_security_locktype_t;

//
//! Security 128b Key
//
typedef union
{
    uint32_t keyword[4];
    struct
    {
        uint32_t key0;
        uint32_t key1;
        uint32_t key2;
        uint32_t key3;
    } keys;
} am_hal_security_128bkey_t;

//
//! Security SOC Id
//
typedef struct
{
    uint32_t socid[AM_HAL_SECURITY_SOCID_NUMWORDS];
} am_hal_security_socid_t;

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @brief  Get Device Security Info
//!
//! @param  pSecInfo -  Pointer to structure for returned security info
//!
//! This will retrieve the security information for the device
//!
//! @return Returns AM_HAL_STATUS_SUCCESS on success
//!
//! @note IMPORTANT - This function requires access to INFO0 and/or INFO1 data.
//! Therefore, if either INFO0 or INFO1 is in OTP, OTP must be powered up prior
//! to calling this function. For more information, see the description for
//! am_hal_info0_read().
//
//*****************************************************************************
extern uint32_t am_hal_security_get_info(am_hal_security_info_t *pSecInfo);

//*****************************************************************************
//
//! @brief  Get Device Security SOCID
//!
//! @param  pSocId -  Pointer to structure for returned SOCID info
//!
//! This will retrieve the SOCID information for the device
//!
//! @return Returns AM_HAL_STATUS_SUCCESS on success
//
//*****************************************************************************
extern uint32_t am_hal_security_get_socid(am_hal_security_socid_t *pSocId);

//*****************************************************************************
//
//! @brief  Set the key for specified lock
//!
//! @param  lockType - The lock type to be operated upon
//! @param  pKey -  Pointer to 128b key value
//!
//! This will program the lock registers for the specified lock and key
//!
//! @return Returns AM_HAL_STATUS_SUCCESS on success
//
//*****************************************************************************
extern uint32_t am_hal_security_set_key(am_hal_security_locktype_t lockType, am_hal_security_128bkey_t *pKey);

//*****************************************************************************
//
//! @brief  Get the current status of the specified lock
//!
//! @param lockType - The lock type to be operated upon
//! @param pbUnlockStatus -  Pointer to return variable with lock status
//!
//! This will get the lock status for specified lock - true implies unlocked
//! Note that except for customer lock, other locks are self-locking on status read
//!
//! @return Returns AM_HAL_STATUS_SUCCESS on success
//
//*****************************************************************************
extern uint32_t am_hal_security_get_lock_status(am_hal_security_locktype_t lockType, bool *pbUnlockStatus);

//*****************************************************************************
//
//! @brief  Compute CRC32 for a specified payload
//!
//! @param  ui32StartAddr - The start address of the payload.
//! @param  ui32SizeBytes - The length of payload in bytes.
//! @param  pui32Crc      - Pointer to variable to return the computed CRC.
//!
//! This function uses the hardware engine to compute CRC32 on an arbitrary data
//! payload.  The payload can reside in any contiguous memory including external
//! memory.
//!
//! @return Returns AM_HAL_STATUS_SUCCESS on success
//
//*****************************************************************************
extern uint32_t am_hal_crc32(uint32_t startAddr, uint32_t sizeBytes, uint32_t *pCrc);

//*****************************************************************************
//
//! @brief  Compute CRC32 for a specified payload with boundary crossing protection
//!
//! @param  pui8StartAddr - Pointer to the start address of the payload.
//! @param  ui32SizeBytes - The length of payload in bytes (must be multiple of 4).
//! @param  pui32Crc      - Pointer to variable to return the computed CRC.
//!
//! This function uses the hardware engine to compute CRC32 on an arbitrary data
//! payload.  The payload can reside in any contiguous memory including external
//! memory. This implementation includes workarounds for when transfers cross
//! certain memory boundaries (DTCM-SSRAM0, SSRAM0-SSRAM1, SSRAM1-SSRAM2)
//! or ERR014 when the CRC crosses 4KB boundaries within TCM. The function
//! automatically splits transfers at these boundaries.
//!
//! @return Returns AM_HAL_STATUS_SUCCESS on success
//
//*****************************************************************************
extern uint32_t am_hal_crc(uint32_t *pui32StartAddr,
                    uint32_t ui32SizeBytes,
                    uint32_t *pui32Crc);

//*****************************************************************************
//
//! @brief  Helper function to Perform exit operations for a secondary bootloader
//
//! @param  pImage - The address of the image to give control to
//
//! This function does the necessary security operations while exiting from a
//! a secondary bootloader program. If still open, it locks the infoc key region,
//! as well as further updates to the flash protection register.
//! It also checks if it needs to halt to honor a debugger request.
//! If an image address is specified, control is transferred to the same on exit.
//!
//! @return Returns AM_HAL_STATUS_SUCCESS on success, if no image address specified
//! If an image address is provided, a successful execution results in transfer to
//! the image - and this function does not return.
//
//*****************************************************************************
extern uint32_t am_hal_bootloader_exit(uint32_t *pImage, bool bEnableDebuggerOnExit);

#ifdef __cplusplus
}
#endif

#endif // AM_HAL_SECURITY_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************

