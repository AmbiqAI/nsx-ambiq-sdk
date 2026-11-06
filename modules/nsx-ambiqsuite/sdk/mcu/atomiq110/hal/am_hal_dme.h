//*****************************************************************************
//
//! @file am_hal_dme.h
//!
//! @brief DME (Data Movement Engine) HAL module.
//!
//! @addtogroup dme_at110 DME - Data Movement Engine
//! @ingroup atomiq110_hal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2026, Ambiq Micro, Inc.
// All rights reserved.
//
// This is part of revision stable-2026.11.05 of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_HAL_DME_H
#define AM_HAL_DME_H

#include <stdint.h>
#include <wchar.h>
#include "am_hal_global.h"

#ifdef __cplusplus
extern "C"
{
#endif

//
//! DME channel Count
//
#define AM_HAL_DME_MAX_CHANNELS    2

//
//! DME channel status byte bits
//! These masks match the channel status register byte in bits [23:16], shifted
//! down into bits [7:0] of the value returned by am_hal_dme_ch_status_get().
//
#define AM_HAL_DME_STAT_DONE         (DMEA_DMACH0CHSTATUS_STATDONE_Msk >> 16)
#define AM_HAL_DME_STAT_ERR          (DMEA_DMACH0CHSTATUS_STATERR_Msk >> 16)
#define AM_HAL_DME_STAT_DISABLED     (DMEA_DMACH0CHSTATUS_STATDISABLED_Msk >> 16)
#define AM_HAL_DME_STAT_STOPPED      (DMEA_DMACH0CHSTATUS_STATSTOPPED_Msk >> 16)
#define AM_HAL_DME_STAT_PAUSED       (DMEA_DMACH0CHSTATUS_STATPAUSED_Msk >> 16)
#define AM_HAL_DME_STAT_RESUMEWAIT   (DMEA_DMACH0CHSTATUS_STATRESUMEWAIT_Msk >> 16)

//
//! DME Channel Command descriptor header mask positions
//
#define AM_HAL_DME_CMD_DESC_MASK_INTREN_POS      (2U)
#define AM_HAL_DME_CMD_DESC_MASK_CTRL_POS        (3U)
#define AM_HAL_DME_CMD_DESC_MASK_SRCADDR_POS     (4U)
#define AM_HAL_DME_CMD_DESC_MASK_DESADDR_POS     (6U)
#define AM_HAL_DME_CMD_DESC_MASK_XSIZE_POS       (8U)
#define AM_HAL_DME_CMD_DESC_MASK_XSIZE_HI_POS    (9U)
#define AM_HAL_DME_CMD_DESC_MASK_SRCTRANSCFG_POS (10U)
#define AM_HAL_DME_CMD_DESC_MASK_DESTRANSCFG_POS (11U)
#define AM_HAL_DME_CMD_DESC_MASK_XADDRINC_POS    (12U)
#define AM_HAL_DME_CMD_DESC_MASK_YADDRSTRIDE_POS (13U)
#define AM_HAL_DME_CMD_DESC_MASK_FILLVAL_POS     (14U)
#define AM_HAL_DME_CMD_DESC_MASK_YSIZE_POS       (15U)
#define AM_HAL_DME_CMD_DESC_MASK_TMPLTCFG_POS    (16U)
#define AM_HAL_DME_CMD_DESC_MASK_SRCTMPLT_POS    (17U)
#define AM_HAL_DME_CMD_DESC_MASK_DESTMPLT_POS    (18U)
#define AM_HAL_DME_CMD_DESC_MASK_LINKATTR_POS    (28U)
#define AM_HAL_DME_CMD_DESC_MASK_AUTOCFG_POS     (29U)
#define AM_HAL_DME_CMD_DESC_MASK_LINK_ADDR_POS   (30U)

//
//! DME Channel Memory Attribute values
//
#define AM_HAL_DME_MEMATTR_LO_INNER_NC (4U)
#define AM_HAL_DME_MEMATTR_HI_OUTER_NC (4U)
#define AM_HAL_DME_SHRATTR_INNER       (3U)
#define AM_HAL_DME_MEMATTR_LO_DEFAULT  (AM_HAL_DME_MEMATTR_LO_INNER_NC)
#define AM_HAL_DME_MEMATTR_HI_DEFAULT  (AM_HAL_DME_MEMATTR_HI_OUTER_NC)
#define AM_HAL_DME_SHRATTR_DEFAULT     (AM_HAL_DME_SHRATTR_INNER)

//
//! DME Channel Privileged attribute
//
typedef enum
{
    AM_HAL_DME_PRIVATTR_UNPRIVILEGED = 0U,
    AM_HAL_DME_PRIVATTR_PRIVILEGED   = 1U,
} am_hal_dme_privattr_e;

//
//! DME Channel Non-secure attribute
//
typedef enum
{
    AM_HAL_DME_NONSECATTR_SECURE    = 0U,
    AM_HAL_DME_NONSECATTR_NONSECURE = 1U,
} am_hal_dme_nonsecattr_e;

//
//! DME Channel X-direction transfer mode
//
typedef enum
{
    AM_HAL_DME_XTYPE_DISABLE  = 0U,
    AM_HAL_DME_XTYPE_CONTINUE = 1U,
    AM_HAL_DME_XTYPE_WRAP     = 2U,
    AM_HAL_DME_XTYPE_FILL     = 3U,
} am_hal_dme_xtype_e;

//
//! DME Channel Y-direction transfer mode
//
typedef enum
{
    AM_HAL_DME_YTYPE_DISABLE  = 0U,
    AM_HAL_DME_YTYPE_CONTINUE = 1U,
    AM_HAL_DME_YTYPE_WRAP     = 2U,
    AM_HAL_DME_YTYPE_FILL     = 3U,
} am_hal_dme_ytype_e;

//
//! DME Channel Transfer size
//
typedef enum
{
    AM_HAL_DME_TRANSIZE_1BYTE  = 0U,
    AM_HAL_DME_TRANSIZE_2BYTE  = 1U,
    AM_HAL_DME_TRANSIZE_4BYTE  = 2U,
    AM_HAL_DME_TRANSIZE_8BYTE  = 3U,
    AM_HAL_DME_TRANSIZE_16BYTE = 4U,
} am_hal_dme_transize_e;

//
//! DME Channel Reload type
//
typedef enum
{
    AM_HAL_DME_RELOADTYPE_DISABLED             = 0U,
    AM_HAL_DME_RELOADTYPE_SIZE                 = 1U,
    AM_HAL_DME_RELOADTYPE_SIZE_SRCADDR         = 3U,
    AM_HAL_DME_RELOADTYPE_SIZE_DESADDR         = 5U,
    AM_HAL_DME_RELOADTYPE_SIZE_SRCADDR_DESADDR = 7U,
} am_hal_dme_reloadtype_e;

//
//! DME Channel Done Type
//!
//! @note For a multi-descriptor command link that uses
//!       am_hal_dme_commandlink_execute() / am_hal_dme_ch_complete_wait(),
//!       set non-final descriptors to AM_HAL_DME_DONETYPE_NOT_ASSERTED and
//!       the final descriptor to AM_HAL_DME_DONETYPE_END_OF_COMMAND so
//!       STAT_DONE is asserted only when the last command completes.
//
typedef enum
{
    AM_HAL_DME_DONETYPE_NOT_ASSERTED       = 0U, //!< Do not assert STAT_DONE for this command.
    AM_HAL_DME_DONETYPE_END_OF_COMMAND     = 1U, //!< Assert STAT_DONE when this command completes.
    AM_HAL_DME_DONETYPE_END_OF_AUTORESTART = 3U, //!< Assert STAT_DONE at the end of an auto-restart sequence.
} am_hal_dme_donetype_e;

//
//! DME Channel priority
//
typedef enum
{
    AM_HAL_DME_CHPRIO_0  = 0U, // Lowest Priority
    AM_HAL_DME_CHPRIO_1  = 1U,
    AM_HAL_DME_CHPRIO_2  = 2U,
    AM_HAL_DME_CHPRIO_3  = 3U,
    AM_HAL_DME_CHPRIO_4  = 4U,
    AM_HAL_DME_CHPRIO_5  = 5U,
    AM_HAL_DME_CHPRIO_6  = 6U,
    AM_HAL_DME_CHPRIO_7  = 7U,
    AM_HAL_DME_CHPRIO_8  = 8U,
    AM_HAL_DME_CHPRIO_9  = 9U,
    AM_HAL_DME_CHPRIO_10 = 10U,
    AM_HAL_DME_CHPRIO_11 = 11U,
    AM_HAL_DME_CHPRIO_12 = 12U,
    AM_HAL_DME_CHPRIO_13 = 13U,
    AM_HAL_DME_CHPRIO_14 = 14U,
    AM_HAL_DME_CHPRIO_15 = 15U, // Highest Priority
} am_hal_dme_chprio_e;


//
//! DME channel access token structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t magic   : 12;
        uint32_t channel : 1;
        uint32_t resv    : 3;
        uint32_t taken   : 1;
        uint32_t session : 15;
    };
} am_hal_dme_ch_access_token_t;

//
//! DME channel ISR callback function type.
//! ui32Status uses AM_HAL_STATUS_xxx / am_hal_dme_status_t codes.
//! This mirrors am_hal_dme_ch_complete_wait() status semantics, except timeout
//! is not used in callback context.
//
typedef void (*am_hal_dme_ch_isr_callback_t)(uint32_t ui32Channel,
                                             uint32_t ui32Status,
                                             void *pContext);

//
//! DME channel command selectors
//
typedef enum
{
    AM_HAL_DME_CH_CMD_ENABLE = 0,     //!< Write CHCMD.ENABLECMD.
    AM_HAL_DME_CH_CMD_CLEAR,          //!< Write CHCMD.CLEARCMD.
    AM_HAL_DME_CH_CMD_DISABLE,        //!< Write CHCMD.DISABLECMD.
    AM_HAL_DME_CH_CMD_STOP,           //!< Write CHCMD.STOPCMD.
    AM_HAL_DME_CH_CMD_PAUSE,          //!< Write CHCMD.PAUSECMD.
    AM_HAL_DME_CH_CMD_RESUME          //!< Write CHCMD.RESUMECMD.
} am_hal_dme_ch_cmd_e;

//
//! DME channel status codes.
//! These status values are reported from the per-channel interrupt vector
//! based on CHSTATUS (and CHERRINFO when STAT_ERR is asserted). Only the
//! status bits enabled via CHINTREN can raise a channel interrupt.
//
typedef enum
{
    AM_HAL_DME_STATUS_BUS_ERROR = AM_HAL_STATUS_MODULE_SPECIFIC_START,  //!< STAT_ERR + CHERRINFO indicates bus access error.
    AM_HAL_DME_STATUS_CONFIG_ERROR,                                     //!< STAT_ERR + CHERRINFO indicates configuration error.
    AM_HAL_DME_STATUS_ERR_OTHER,                                        //!< STAT_ERR asserted but CHERRINFO did not indicate BUSERR or CFGERR.
    AM_HAL_DME_STATUS_DISABLED,                                         //!< STAT_DISABLED asserted (channel reached disabled state).
    AM_HAL_DME_STATUS_STOPPED,                                          //!< STAT_STOPPED asserted (channel reached stopped state).
    AM_HAL_DME_STATUS_PAUSED,                                           //!< STAT_PAUSED asserted (channel reached paused state).
    AM_HAL_DME_STATUS_RESUMEWAIT,                                       //!< STAT_RESUMEWAIT asserted (waiting for resume).
    AM_HAL_DME_STATUS_SRCTRIGINWAIT,                                    //!< STAT_SRCTRIGINWAIT asserted (waiting for source trigger).
    AM_HAL_DME_STATUS_DESTRIGINWAIT,                                    //!< STAT_DESTRIGINWAIT asserted (waiting for destination trigger).
    AM_HAL_DME_STATUS_TRIGOUTACKWAIT,                                   //!< STAT_TRIGOUTACKWAIT asserted (waiting for trigger-out acknowledge).
    // Note: Keep AM_HAL_DME_STATUS_END the last entry in this enum list.
    AM_HAL_DME_STATUS_END,                                              //!< Keep this the last entry in this enum list.
} am_hal_dme_status_t;

//
// ! DME channel interrupt enable register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t done           : 1;  // Enables interrupt generation when STAT_DONE is asserted.
        uint32_t err            : 1;  // Enables interrupt generation when STAT_ERR is asserted.
        uint32_t disabled       : 1;  // Enables interrupt generation when STAT_DISABLED is asserted.
        uint32_t stopped        : 1;  // Enables interrupt generation when STAT_STOPPED is asserted.
        uint32_t                : 4;
        uint32_t srctriginwait  : 1;  // Enables interrupt generation while waiting for a source trigger.
        uint32_t destriginwait  : 1;  // Enables interrupt generation while waiting for a destination trigger.
        uint32_t trigoutackwait : 1;  // Enables interrupt generation while waiting for trigger output acknowledge.
        uint32_t                : 21;
    };
} am_hal_dme_ch_intren_t;

//
//! DME channel control register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t transize      : 3;  // Selects the transfer unit size in bytes as 2^TRANSIZE.
        uint32_t               : 1;
        uint32_t chprio        : 4;  // Sets the channel priority from lowest (0) to highest (15).
        uint32_t               : 1;
        uint32_t xtype         : 3;  // Selects the X-direction transfer mode such as disable, continue, wrap, or fill.
        uint32_t ytype         : 3;  // Selects the Y-direction transfer mode for 1D/2D operation behavior.
        uint32_t               : 3;
        uint32_t regreloadtype : 3;  // Controls which initial registers are automatically reloaded when the command completes.
        uint32_t donetype      : 3;  // Selects when STAT_DONE is asserted; use NOT_ASSERTED on non-final linked commands.
        uint32_t donepauseen   : 1;  // Requests an automatic pause when STAT_DONE is asserted.
        uint32_t usesrctrigin  : 1;  // Enables source trigger input usage for this command.
        uint32_t usedestrigin  : 1;  // Enables destination trigger input usage for this command.
        uint32_t usetrigout    : 1;  // Enables trigger output usage for this command.
        uint32_t               : 4;
    };
} am_hal_dme_ch_ctrl_t;

//
//! DME channel X size (lower 16-bits) register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t src : 16;  // Lower 16 bits of the source X transfer count.
        uint32_t des : 16;  // Lower 16 bits of the destination X transfer count.
    };
} am_hal_dme_ch_xsize_t;

//
//! DME channel X size (higher 16-bits) register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t src : 16;  // Upper 16 bits of the source X transfer count.
        uint32_t des : 16;  // Upper 16 bits of the destination X transfer count.
    };
} am_hal_dme_ch_xsizehi_t;

//
//! DME channel source transfer configuration register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t memattrlo   : 4;  // Selects the low memory-attribute bits for source reads.
        uint32_t memattrhi   : 4;  // Selects the high memory-attribute bits for source reads.
        uint32_t shareattr   : 2;  // Selects the shareability attribute for source reads.
        uint32_t nonsecattr  : 1;  // Selects secure or non-secure attribute for source reads.
        uint32_t privattr    : 1;  // Selects privileged or unprivileged attribute for source reads.
        uint32_t             : 4;
        uint32_t maxburstlen : 4;  // Hints the maximum burst length the DMA may use for source reads.
        uint32_t             : 12;
    };
} am_hal_dme_ch_srctranscfg_t;

//
//! DME channel destination transfer configuration register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t memattrlo   : 4;  // Selects the low memory-attribute bits for destination writes.
        uint32_t memattrhi   : 4;  // Selects the high memory-attribute bits for destination writes.
        uint32_t shareattr   : 2;  // Selects the shareability attribute for destination writes.
        uint32_t nonsecattr  : 1;  // Selects secure or non-secure attribute for destination writes.
        uint32_t privattr    : 1;  // Selects privileged or unprivileged attribute for destination writes.
        uint32_t             : 4;
        uint32_t maxburstlen : 4;  // Hints the maximum burst length the DMA may use for destination writes.
        uint32_t             : 12;
    };
} am_hal_dme_ch_destranscfg_t;

//
//! DME channel X address increment register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t src : 16;  // Per-transfer source address increment in TRANSIZE-scaled units.
        uint32_t des : 16;  // Per-transfer destination address increment in TRANSIZE-scaled units.
    };
} am_hal_dme_ch_xaddrinc_t;

//
//! DME channel Y address stride register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t src : 16;  // Source line stride applied after each completed Y line.
        uint32_t des : 16;  // Destination line stride applied after each completed Y line.
    };
} am_hal_dme_ch_yaddrstride_t;

//
//! DME channel Y size register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t src : 16;  // Source Y dimension, or number of source lines.
        uint32_t des : 16;  // Destination Y dimension, or number of destination lines.
    };
} am_hal_dme_ch_ysize_t;

//
//! DME channel template configuration register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t         : 8;
        uint32_t srcsize : 5;  // Selects the source template size in transfers plus one.
        uint32_t         : 3;
        uint32_t dessize : 5;  // Selects the destination template size in transfers plus one.
        uint32_t         : 11;
    };
} am_hal_dme_ch_tmpltcfg_t;

//
//! DME channel source template register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t lsb : 1;  // Read-only least-significant source template bit, fixed to 1.
        uint32_t val : 31; // Source packing template pattern used for source accesses.
    };
} am_hal_dme_ch_srctmplt_t;

//
//! DME channel destination template register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t lsb : 1;  // Read-only least-significant destination template bit, fixed to 1.
        uint32_t val : 31; // Destination packing template pattern used for destination accesses.
    };
} am_hal_dme_ch_destmplt_t;

//
//! DME channel link attribute register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t memattrlo : 4;  // Selects the low memory-attribute bits for link-address reads.
        uint32_t memattrhi : 4;  // Selects the high memory-attribute bits for link-address reads.
        uint32_t shareattr : 2;  // Selects the shareability attribute for link-address reads.
        uint32_t           : 22;
    };
} am_hal_dme_ch_linkattr_t;

//
//! DME channel automatic restart configuration register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t restartcnt   : 16;  // Sets how many automatic command restarts occur before linking or finishing.
        uint32_t restartinfen : 1;   // Enables infinite automatic command restart until explicitly stopped or disabled.
        uint32_t              : 15;
    };
} am_hal_dme_ch_autocfg_t;

//
//! DME channel link address register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t en   : 1;  // Enables fetching the next command from the link address when this command completes.
        uint32_t      : 1;
        uint32_t addr : 30; // Holds the next command link pointer in bits [31:2].
    };
} am_hal_dme_ch_linkaddr_t;

//
//! DME channel error information register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t buserr          : 1;  // DMA bus error occurred during data or command read transfer.
        uint32_t cfgerr          : 1;  // DMA command configuration is not supported by the implementation.
        uint32_t srctriginselerr : 1;  // Selected source trigger is not allowed for this channel.
        uint32_t destriginselerr : 1;  // Selected destination trigger is not allowed for this channel.
        uint32_t trigoutselerr   : 1;  // Selected trigger output is not allowed for this channel.
        uint32_t                 : 11;
        uint32_t axirdresperr    : 1;  // AXI read transaction returned an error response.
        uint32_t axiwrresperr    : 1;  // AXI write transaction returned an error response.
        uint32_t axirdpoiserr    : 1;  // AXI read transaction returned poisoned data.
        uint32_t                 : 5;
        uint32_t linkhdrerr      : 1;  // Linked command header is invalid.
        uint32_t regvalerr       : 1;  // A configuration register contains an illegal field value.
        uint32_t cfgconflerr     : 1;  // Configuration register settings conflict with each other.
        uint32_t                 : 5;
    };
} am_hal_dme_ch_errinfo_t;

//
//! DME channel Command Descriptor mask register access structure
//
typedef union
{
    uint32_t u32;
    struct
    {
        uint32_t regclear_msk    : 1;  // 1 = enable register-clear before new register values are loaded.
        uint32_t                 : 1;
        uint32_t intren_msk      : 1;  // 1 = enable config writing for INTREN.
        uint32_t ctrl_msk        : 1;  // 1 = enable config writing for CTRL.
        uint32_t srcaddr_msk     : 1;  // 1 = enable config writing for SRCADDR.
        uint32_t                 : 1;
        uint32_t desaddr_msk     : 1;  // 1 = enable config writing for DESADDR.
        uint32_t                 : 1;
        uint32_t xsize_msk       : 1;  // 1 = enable config writing for XSIZE.
        uint32_t xsizehi_msk     : 1;  // 1 = enable config writing for XSIZEHI.
        uint32_t srctranscfg_msk : 1;  // 1 = enable config writing for SRCTRANSCFG.
        uint32_t destranscfg_msk : 1;  // 1 = enable config writing for DESTRANSCFG.
        uint32_t xaddrinc_msk    : 1;  // 1 = enable config writing for XADDRINC.
        uint32_t yaddrstride_msk : 1;  // 1 = enable config writing for YADDRSTRIDE.
        uint32_t fillval_msk     : 1;  // 1 = enable config writing for FILLVAL.
        uint32_t ysize_msk       : 1;  // 1 = enable config writing for YSIZE.
        uint32_t tmpltcfg_msk    : 1;  // 1 = enable config writing for TMPLTCFG.
        uint32_t srctmplt_msk    : 1;  // 1 = enable config writing for SRCTMPLT.
        uint32_t destmplt_msk    : 1;  // 1 = enable config writing for DESTMPLT.
        uint32_t                 : 9;
        uint32_t linkattr_msk    : 1;  // 1 = enable config writing for LINKATTR.
        uint32_t autocfg_msk     : 1;  // 1 = enable config writing for AUTOCFG.
        uint32_t linkaddr_msk    : 1;  // 1 = enable config writing for LINKADDR.
        uint32_t                 : 1;
    } mask_b;

} am_hal_dme_ch_config_mask_t;

//
// ! DME channel configuration structure
//
typedef struct
{
    am_hal_dme_ch_config_mask_t mask;
    am_hal_dme_ch_intren_t intren;
    am_hal_dme_ch_ctrl_t ctrl;
    uint32_t srcaddr;
    uint32_t desaddr;
    am_hal_dme_ch_xsize_t xsize;
    am_hal_dme_ch_xsizehi_t xsizehi;
    am_hal_dme_ch_srctranscfg_t srctranscfg;
    am_hal_dme_ch_destranscfg_t destranscfg;
    am_hal_dme_ch_xaddrinc_t xaddrinc;
    am_hal_dme_ch_yaddrstride_t yaddrstride;
    uint32_t fillval;
    am_hal_dme_ch_ysize_t ysize;
    am_hal_dme_ch_tmpltcfg_t tmpltcfg;
    am_hal_dme_ch_srctmplt_t srctmplt;
    am_hal_dme_ch_destmplt_t destmplt;
    am_hal_dme_ch_linkattr_t linkattr;
    am_hal_dme_ch_autocfg_t autocfg;
    am_hal_dme_ch_linkaddr_t linkaddr;
} am_hal_dme_ch_config_t;

//*****************************************************************************
//
//! @brief Default DME channel configuration template.
//
//! This constant provides a ready-to-copy baseline configuration
//
//*****************************************************************************
extern const am_hal_dme_ch_config_t g_am_hal_dme_ch_config_default;

// #### INTERNAL BEGIN ####
//*****************************************************************************
//
//! @brief Pause all DME channels in the caller's security domain.
//!
//! Secure TZ and non-TZ builds use SEC_CTRL; Non-secure TZ builds
//! (AM_HAL_TZ_NONSECURE) use NSEC_CTRL.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//!
//! @note Affects channels in the caller's domain only. Secure software that
//!       must pause Non-secure channels should call
//!       am_hal_dme_all_ch_pause_ns().
//
//*****************************************************************************
extern uint32_t am_hal_dme_all_ch_pause(void);

//*****************************************************************************
//
//! @brief Stop all DME channels in the caller's security domain.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//!
//! @note Affects channels in the caller's domain only.
//
//*****************************************************************************
extern uint32_t am_hal_dme_all_ch_stop(void);

//*****************************************************************************
//
//! @brief Read all-channel paused status for the caller's security domain.
//!
//! @param pbPaused - Receives true when STATALLCHPAUSED is asserted.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!
//! @note Reports the caller's domain only. Secure/non-TZ software that must
//!       observe Non-secure pause state should call
//!       am_hal_dme_all_ch_paused_ns().
//
//*****************************************************************************
extern uint32_t am_hal_dme_all_ch_paused(bool *pbPaused);

//*****************************************************************************
//
//! @brief Read all-channel stopped status for the caller's security domain.
//!
//! @param pbStopped - Receives true when STATALLCHSTOPPED is asserted.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!
//! @note Reports the caller's domain only.
//
//*****************************************************************************
extern uint32_t am_hal_dme_all_ch_stopped(bool *pbStopped);

//*****************************************************************************
//
//! @brief Clear all-channel paused status and resume channels in the caller's
//!        security domain.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//!
//! @note Affects channels in the caller's domain only.
//
//*****************************************************************************
extern uint32_t am_hal_dme_all_ch_pause_resume(void);

//*****************************************************************************
//
//! @brief Clear all-channel stopped status for the caller's security domain.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//!
//! @note Affects channels in the caller's domain only.
//
//*****************************************************************************
extern uint32_t am_hal_dme_all_ch_stop_clear(void);

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE & 0x2)
//*****************************************************************************
//
//! @brief Pause all Non-secure-domain DME channels (NSEC_CTRL.ALLCHPAUSE).
//!
//! Built for Secure TZ only.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//!
//! @note Affects Non-secure channels only. Use am_hal_dme_all_ch_pause() for
//!       the caller's (Secure) domain.
//
//*****************************************************************************
extern uint32_t am_hal_dme_all_ch_pause_ns(void);

//*****************************************************************************
//
//! @brief Read Non-secure-domain all-channel paused status
//!        (NSECSTATUS.STATALLCHPAUSED).
//!
//! Built for Secure TZ only.
//!
//! @param pbPaused - Receives true when STATALLCHPAUSED is asserted.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!
//! @note Reports Non-secure channels only.
//
//*****************************************************************************
extern uint32_t am_hal_dme_all_ch_paused_ns(bool *pbPaused);

//*****************************************************************************
//
//! @brief Clear Non-secure-domain all-channel paused status and resume
//!        Non-secure channels.
//!
//! Built for Secure TZ only.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//!
//! @note Affects Non-secure channels only.
//
//*****************************************************************************
extern uint32_t am_hal_dme_all_ch_pause_resume_ns(void);
#endif // defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE & 0x2)

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE & 0x2)
//*****************************************************************************
//
//! @brief Configure whether a DME channel is non-secure.
//!
//! Programs DMA-350 @c SCFGCHSEC only. This does **not** update NVIC ITNS
//! (Interrupt Target Non-Secure) for any DME IRQ line:
//!   - Common IRQs (@c SYSDMECOMMONSECURE / @c SYSDMECOMMONNONSECURE) are
//!     separate lines; ITNS for those is typically a one-time Secure boot
//!     policy and does not need remapping on each channel security change.
//!   - Per-channel IRQs (@c SYSDMECHANNELn) are a single IRQ number per
//!     channel. ARM requires ITNS alignment with channel security when those
//!     lines are used - higher layers (Secure firmware / application) must
//!     disable, clear-pending, retarget ITNS, and re-enable as appropriate.
//!
//! Channel privilege (@c CHPRIV) is frame-local (Secure vs Non-secure) and is
//! **not** transferred when security changes. After calling this function,
//! call @c am_hal_dme_ch_priv_set() again to set privilege in the new domain.
//!
//! @param ui32Channel - DME channel index (0 to AM_HAL_DME_MAX_CHANNELS - 1).
//! @param bNonSecure  - true to mark the channel non-secure, false for secure.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if ui32Channel is out of range.
//!         AM_HAL_STATUS_FAIL if the programmed value does not read back.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_ns_set(uint32_t ui32Channel, bool bNonSecure);
#endif // defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE & 0x2)

//*****************************************************************************
//
//! @brief Configure whether a DME channel is privileged.
//!
//! This controls DMA channel APB/register privilege access only; it does not
//! imply privileged filtering for system memory transfers. Attempts to query or
//! modify inaccessible Secure channel state from Non-secure software may result
//! in ineffective writes or invalid readback values due to RAZ/WI behavior.
//!
//! Privilege is programmed according to the channel's **current** security
//! domain (@c SECCHCFG or @c NSECCHCFG). When channel security is changed via
//! @c am_hal_dme_ch_ns_set(), privilege is not transferred to the other domain;
//! call this function again after the security change.
//!
//! @param ui32Channel  - DME channel index (0 to AM_HAL_DME_MAX_CHANNELS - 1).
//! @param bPrivileged  - true for privileged, false for unprivileged.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if ui32Channel is out of range.
//!         AM_HAL_STATUS_FAIL if the programmed value does not read back.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_priv_set(uint32_t ui32Channel, bool bPrivileged);

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE & 0x2)
//*****************************************************************************
//
//! @brief Query whether a DME channel is non-secure.
//!
//! @param ui32Channel  - DME channel index (0 to AM_HAL_DME_MAX_CHANNELS - 1).
//! @param pbNonSecure  - Receives true when the channel is non-secure.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_ns_get(uint32_t ui32Channel, bool *pbNonSecure);
#endif // defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE & 0x2)

//*****************************************************************************
//
//! @brief Query whether a DME channel is privileged.
//!
//! This reports DMA channel APB/register privilege state only; it does not
//! describe system memory access privilege. Attempts to query inaccessible
//! Secure channel state from Non-secure software may return invalid readback
//! values due to RAZ behavior.
//!
//! @param ui32Channel   - DME channel index (0 to AM_HAL_DME_MAX_CHANNELS - 1).
//! @param pbPrivileged  - Receives true when the channel is privileged.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_priv_get(uint32_t ui32Channel, bool *pbPrivileged);

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE & 0x2)
//*****************************************************************************
//
//! @brief Restore tracked TrustZone / privilege configuration to hardware.
//!
//! Re-applies the Secure track-on-write shadow (@c SCFGCHSEC0, per-frame
//! @c CHPRIV, and @c SCFGCTRL policy bits) after DME power-loss. No-op success
//! when nothing has been tracked, or when @c SCFGCTRL.SECCFGLCK is already set
//! (locked until reset; SCFG is read-only and hardware still holds the config).
//!
//! Restore order when unlocked: @c SCFGCHSEC0, then Secure/Non-secure
//! @c CHPRIV, then @c SCFGCTRL (so @c SECCFGLCK, if shadowed, is written last).
//!
//! @return AM_HAL_STATUS_SUCCESS on success, when already locked, or when no
//!         configuration has been tracked (no-op).
//
//*****************************************************************************
extern uint32_t am_hal_dme_sec_config_restore(void);
#endif // defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE & 0x2)

//*****************************************************************************
//
//! @brief Enable secure access violation interrupt and NVIC line.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//
//*****************************************************************************
extern uint32_t am_hal_dme_secaccvio_interrupt_enable(void);

//*****************************************************************************
//
//! @brief Disable secure access violation interrupt and NVIC line.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//
//*****************************************************************************
extern uint32_t am_hal_dme_secaccvio_interrupt_disable(void);

//*****************************************************************************
//
//! @brief Read secure access violation interrupt status.
//!
//! @param pui32Status - Receives SCFGINTRSTATUS.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if pui32Status is NULL.
//
//*****************************************************************************
extern uint32_t am_hal_dme_secaccvio_interrupt_stat_get(uint32_t *pui32Status);

//*****************************************************************************
//
//! @brief Clear secure access violation interrupt status.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//
//*****************************************************************************
extern uint32_t am_hal_dme_secaccvio_interrupt_clear(void);

//*****************************************************************************
//
//! @brief Enable secure channel common interrupt and NVIC line.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//
//*****************************************************************************
extern uint32_t am_hal_dme_s_ch_interrupt_enable(void);

//*****************************************************************************
//
//! @brief Disable secure channel common interrupt and NVIC line.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//
//*****************************************************************************
extern uint32_t am_hal_dme_s_ch_interrupt_disable(void);

//*****************************************************************************
//
//! @brief Read secure channel common interrupt status.
//!
//! @param pui32Status - Receives bitmap of secure channels raising interrupt.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if pui32Status is NULL.
//
//*****************************************************************************
extern uint32_t am_hal_dme_s_ch_interrupt_stat_get(uint32_t *pui32Status);

//*****************************************************************************
//
//! @brief Enable non-secure channel common interrupt and NVIC line.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ns_ch_interrupt_enable(void);

//*****************************************************************************
//
//! @brief Disable non-secure channel common interrupt and NVIC line.
//!
//! @return AM_HAL_STATUS_SUCCESS.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ns_ch_interrupt_disable(void);

//*****************************************************************************
//
//! @brief Read non-secure channel common interrupt status.
//!
//! @param pui32Status - Receives bitmap of non-secure channels raising interrupt.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if pui32Status is NULL.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ns_ch_interrupt_stat_get(uint32_t *pui32Status);
// #### INTERNAL END ####

//*****************************************************************************
//
//! @brief Open a DME channel and get its access token.
//!
//! @param ui32Channel - DME channel index (0 to AM_HAL_DME_MAX_CHANNELS - 1).
//! @param pToken    - Pointer to store the channel access token (output).
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         AM_HAL_STATUS_IN_USE if the channel is already open.
//!         Other @c AM_HAL_STATUS_* values from @c am_hal_pwrctrl_periph_enable()
//!         if the first-open power hold fails.
//!
//! @note The caller that obtains the token must ensure that the token is not
//!       used for API access concurrently by more than one thread.
//! @note The first successful open enables @c AM_HAL_PWRCTRL_PERIPH_DME so
//!       MCUL/DME remain powered across deep sleep while any channel is open.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_open(uint32_t ui32Channel, am_hal_dme_ch_access_token_t *pToken);

//*****************************************************************************
//
//! @brief Close a DME channel and release its access token.
//!
//! @param pToken - Access token returned from am_hal_dme_ch_open().
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_HANDLE if pToken is invalid.
//!         Other @c AM_HAL_STATUS_* values from @c am_hal_pwrctrl_periph_disable()
//!         if the last-close power release fails.
//!
//! @note Closing the last open channel disables @c AM_HAL_PWRCTRL_PERIPH_DME.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_close(am_hal_dme_ch_access_token_t *pToken);

//*****************************************************************************
//
//! @brief Clear the DME channel internal state.
//!
//! @param pToken - Channel token returned from am_hal_dme_ch_open().
//!
//! @return AM_HAL_STATUS_SUCCESS when the CLEAR command bit in CHCMD clears.
//!         AM_HAL_STATUS_INVALID_HANDLE if pToken is invalid.
//!         AM_HAL_STATUS_TIMEOUT if the CLEAR command bit does not clear
//!                               before the wait times out.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_clear(am_hal_dme_ch_access_token_t *pToken);

//*****************************************************************************
//
//! @brief Fill a DME channel configuration structure with default values.
//!
//! @param psConfig - Pointer to the configuration structure to initialize.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_config_init(am_hal_dme_ch_config_t *psConfig);

//*****************************************************************************
//
//! @brief Apply a channel configuration to the selected DME channel.
//!
//! @param pToken - Channel token returned from am_hal_dme_ch_open().
//! @param psConfig - Configuration values and per-register write mask.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_HANDLE if pToken is invalid.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_config(am_hal_dme_ch_access_token_t *pToken,
                                     const am_hal_dme_ch_config_t *psConfig);

//*****************************************************************************
//
//! @brief Send a command word to the selected DME channel CHCMD register.
//!
//! @param pToken - Channel token returned from am_hal_dme_ch_open().
//! @param eCommand - Command selector to assert in CHCMD.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_HANDLE if pToken is invalid.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_command_send(am_hal_dme_ch_access_token_t *pToken,
                                           am_hal_dme_ch_cmd_e eCommand);

//*****************************************************************************
//
//! @brief Wait until any requested DME channel command bit becomes deasserted.
//!
//! @param pToken - Channel token returned from am_hal_dme_ch_open().
//! @param ui32CommandMask - Any channel command register bits to wait to go
//!                          low.
//! @param ui32TimeoutUs - Timeout in microseconds. A value of 0 uses 1000 us.
//!
//! @return AM_HAL_STATUS_SUCCESS if the requested command bits become
//!         deasserted.
//!         AM_HAL_STATUS_TIMEOUT if they do not clear before the timeout
//!         expires.
//!         AM_HAL_STATUS_INVALID_HANDLE if pToken is invalid.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_command_wait_clear(am_hal_dme_ch_access_token_t *pToken,
                                                 uint32_t ui32CommandMask,
                                                 uint32_t ui32TimeoutUs);

//*****************************************************************************
//
//! @brief Read the selected DME channel status byte.
//!
//! @param pToken - Channel token returned from am_hal_dme_ch_open().
//! @param pui32Status - Receives CHSTATUS bits [23:16] shifted into bits [7:0].
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_HANDLE if pToken is invalid.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_status_get(am_hal_dme_ch_access_token_t *pToken,
                                         uint32_t *pui32Status);

//*****************************************************************************
//
//! @brief Wait until a DME channel terminal completion source becomes asserted.
//!
//! @param pToken - Channel token returned from am_hal_dme_ch_open().
//! @param ui32TimeoutUs - Timeout in microseconds. A value of 0 uses 1000 us.
//!
//! @note This function returns on the first terminal CHSTATUS event
//!       (STAT_DONE, STAT_ERR, STAT_DISABLED, or STAT_STOPPED). It does not
//!       inherently wait for an entire command link. For linked commands,
//!       program non-final descriptors with
//!       AM_HAL_DME_DONETYPE_NOT_ASSERTED and the final descriptor with
//!       AM_HAL_DME_DONETYPE_END_OF_COMMAND so STAT_DONE occurs only after
//!       the last command.
//!
//! @return AM_HAL_STATUS_SUCCESS when STAT_DONE is asserted.
//!         AM_HAL_STATUS_TIMEOUT when no terminal completion source is asserted
//!         before the timeout expires.
//!         AM_HAL_STATUS_INVALID_HANDLE if pToken is invalid.
//!         AM_HAL_DME_STATUS_BUS_ERROR if STAT_ERR is asserted and CHERRINFO
//!         indicates BUSERR.
//!         AM_HAL_DME_STATUS_CONFIG_ERROR if STAT_ERR is asserted and
//!         CHERRINFO indicates CFGERR.
//!         AM_HAL_DME_STATUS_ERR_OTHER if STAT_ERR is asserted but CHERRINFO
//!         does not indicate BUSERR or CFGERR.
//!         AM_HAL_DME_STATUS_DISABLED if STAT_DISABLED is asserted.
//!         AM_HAL_DME_STATUS_STOPPED if STAT_STOPPED is asserted.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_complete_wait(am_hal_dme_ch_access_token_t *pToken,
                                            uint32_t ui32TimeoutUs);

//*****************************************************************************
//
//! @brief Read the selected DME channel error information register.
//!
//! @param pToken - Channel token returned from am_hal_dme_ch_open().
//! @param psErrInfo - Receives the CHERRINFO register value decoded into fields.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_HANDLE if pToken is invalid.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_err_info_get(am_hal_dme_ch_access_token_t *pToken,
                                           am_hal_dme_ch_errinfo_t *psErrInfo);

//*****************************************************************************
//
//! @brief Generate DME command descriptor according to the configuration
//!        passed in.
//!
//! @param psConfig - Configuration used to build the descriptor.
//! @param pCmdDescriptor - Buffer to receive the command descriptor words.
//! @param pCmdDescLen - On input, capacity of @p pCmdDescriptor in DWORDs (32-bit words); on
//!                      success, updated to the number of DWORDs written.
//!
//! @note For cacheable memory, callers must flush the descriptor buffer after
//!       writing it and before the DME fetches it.
//!
//! @return AM_HAL_STATUS_SUCCESS when the descriptor is generated.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         AM_HAL_STATUS_OUT_OF_RANGE if the buffer is too small.
//
//*****************************************************************************
extern uint32_t am_hal_dme_command_descriptor_generate(const am_hal_dme_ch_config_t *psConfig,
                                                       uint32_t *pCmdDescriptor,
                                                       uint32_t *pCmdDescLen);

//*****************************************************************************
//
//! @brief Update the link address in the command descriptor.
//!
//! @param pCmdDescriptor - Command descriptor whose link word is updated.
//! @param ui32LinkAddr - New nonzero, 32-bit aligned link address.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
extern uint32_t am_hal_dme_command_descriptor_update_linkaddr(uint32_t *pCmdDescriptor,
                                                              uint32_t ui32LinkAddr);

//*****************************************************************************
//
//! @brief Register or unregister ISR callback for a DME channel.
//!
//! @param pToken - Channel token from am_hal_dme_ch_open().
//! @param pfnCallback - Callback function. Pass NULL to unregister callback.
//! @param pContext - User context passed to @p pfnCallback.
//!
//! @return AM_HAL_STATUS_SUCCESS on success.
//!         AM_HAL_STATUS_INVALID_HANDLE if channel access token is invalid.
//
//*****************************************************************************
extern uint32_t am_hal_dme_ch_interrupt_register_callback(am_hal_dme_ch_access_token_t *pToken,
                                                          am_hal_dme_ch_isr_callback_t pfnCallback,
                                                          void *pContext);

//*****************************************************************************
//
//! @brief Enable DME channel operation and wait until a terminal completion
//!        source is asserted in CHSTATUS. The completion wait happens only when
//!        no ISR callback is registered for the channel session.
//!
//! @param pToken - Channel token from am_hal_dme_ch_open().
//! @param ui32TimeoutUs - Wait timeout in microseconds; 0 selects the default.
//!
//! @note For cacheable memory, callers must flush any source buffers before
//!       execution and invalidate destination buffers before reading back data
//!       written by the DME.
//!
//! @return AM_HAL_STATUS_SUCCESS when the channel is enabled.
//!         AM_HAL_STATUS_INVALID_HANDLE if channel access token is invalid.
//!         AM_HAL_STATUS_TIMEOUT if the channel does not enable or no
//!         terminal completion source is asserted before the timeout expires.
//!         When completion wait happens, CHSTATUS-derived outcomes match
//!         am_hal_dme_ch_complete_wait(); see that function for
//!         AM_HAL_DME_STATUS_xxx and related return codes.
//
//*****************************************************************************
extern uint32_t am_hal_dme_command_execute(am_hal_dme_ch_access_token_t *pToken,
                                           uint32_t ui32TimeoutUs);

//*****************************************************************************
//
//! @brief Execute a command link and wait until a terminal completion source is
//!        asserted in CHSTATUS. The completion wait happens only when no ISR
//!        callback is registered for the channel session.
//!
//! @param pToken - Channel token from am_hal_dme_ch_open().
//! @param pCmdDescriptor - Command descriptor to execute.
//! @param ui32TimeoutUs - Wait timeout in microseconds; 0 selects the default.
//!
//! @note Completion wait returns on the first terminal CHSTATUS event, not
//!       automatically at the end of every linked chain. To wait until the
//!       final command completes successfully, set
//!       AM_HAL_DME_DONETYPE_NOT_ASSERTED on every non-final descriptor and
//!       AM_HAL_DME_DONETYPE_END_OF_COMMAND on the last descriptor. If an
//!       intermediate command asserts STAT_DONE, the wait can return before
//!       later descriptors run.
//!
//! @note For cacheable memory, callers must flush the descriptor buffer and
//!       any source buffers before execution, and invalidate destination
//!       buffers before reading back DME-written data.
//!
//! @return AM_HAL_STATUS_SUCCESS when the command link is started.
//!         AM_HAL_STATUS_INVALID_HANDLE if channel access token is invalid.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         AM_HAL_STATUS_TIMEOUT if the command link does not execute or no
//!         terminal completion source is asserted before the timeout expires.
//!         When no ISR callback is registered, CHSTATUS-derived outcomes match
//!         am_hal_dme_ch_complete_wait(); see that function for
//!         AM_HAL_DME_STATUS_xxx and related return codes.
//
//*****************************************************************************
extern uint32_t am_hal_dme_commandlink_execute(am_hal_dme_ch_access_token_t *pToken,
                                               uint32_t *pCmdDescriptor,
                                               uint32_t ui32TimeoutUs);
#ifdef __cplusplus
}
#endif

#endif // AM_HAL_DME_H

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
