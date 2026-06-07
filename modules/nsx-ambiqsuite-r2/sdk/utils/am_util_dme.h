//*****************************************************************************
//
//! @file am_util_dme.h
//!
//! @brief DMA-350 utility helpers built on the DME HAL.
//!
//! @addtogroup dma_350_utils DMA-350 Utility Functions
//! @ingroup utils
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
//! ${copyright}
//!
//! This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef AM_UTIL_DME_H
#define AM_UTIL_DME_H

#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! @brief 2D copy mode enumeration.
//
//*****************************************************************************
typedef enum
{
    AM_UTIL_DME_2D_COPY_MODE_ORIGINAL,
    AM_UTIL_DME_2D_COPY_MODE_ROTATE_90_CW,
    AM_UTIL_DME_2D_COPY_MODE_ROTATE_180_CW,
    AM_UTIL_DME_2D_COPY_MODE_ROTATE_270_CW,
    AM_UTIL_DME_2D_COPY_MODE_FLIP_HORIZONTAL,
    AM_UTIL_DME_2D_COPY_MODE_FLIP_VERTICAL,
    AM_UTIL_DME_2D_COPY_MODE_FLIP_DIAGONAL,
    AM_UTIL_DME_2D_COPY_MODE_FLIP_ANTI_DIAGONAL,
} am_util_dme_2d_copy_mode_e;

//*****************************************************************************
//
//! @brief 2D copy configuration structure.
//
//*****************************************************************************
typedef struct
{
    uint32_t ui32SrcAddr;                 //!< Top-left source element.
    uint32_t ui32DstAddr;                 //!< Top-left destination element of the transformed output region.
    uint32_t ui32SrcXSize;                //!< Source image width in bytes.
    uint32_t ui32SrcYSize;                //!< Source image height in lines.
    uint32_t ui32DstXSize;                //!< Destination boundary width in bytes. Output is truncated to this width.
    uint32_t ui32DstYSize;                //!< Destination boundary height in lines. Output is truncated to this height.
    uint32_t ui32SrcLineWidth;            //!< Source line pitch in bytes.
    uint32_t ui32DstLineWidth;            //!< Destination line pitch in bytes.
    am_util_dme_2d_copy_mode_e eCopyMode; //!< Copy mode specifying the transformation to apply.
    am_hal_dme_transize_e eTransSize;     //!< Transfer element size. This is used to determine the group size
                                          //!< when transformation is applied.
} am_util_dme_memcpy_2d_config_t;

//*****************************************************************************
//
//! @brief 1D wrapped-copy configuration structure.
//
//*****************************************************************************
typedef struct
{
    uint32_t ui32SrcAddr; //!< Start of the source pattern/window.
    uint32_t ui32SrcLen;  //!< Source pattern/window length in bytes before wrapping.
    uint32_t ui32DstAddr; //!< Start of the destination range.
    uint32_t ui32DstLen;  //!< Total destination length in bytes.
} am_util_dme_1d_wrapcpy_config_t;

//*****************************************************************************
//
//! @brief 2D wrapped-copy configuration structure.
//
//*****************************************************************************
typedef struct
{
    uint32_t ui32SrcAddr;                 //!< Address of top-left source element.
    uint32_t ui32DstAddr;                 //!< Address of top-left destination element.
    uint32_t ui32SrcXSize;                //!< Source width in bytes.
    uint32_t ui32SrcYSize;                //!< Source height in lines.
    uint32_t ui32DstXSize;                //!< Destination width in bytes.
    uint32_t ui32DstYSize;                //!< Destination height in lines.
    uint32_t ui32SrcLineWidth;            //!< Source line pitch in bytes.
    uint32_t ui32DstLineWidth;            //!< Destination line pitch in bytes.
    am_util_dme_2d_copy_mode_e eCopyMode; //!< Copy mode specifying the transformation to apply.
    am_hal_dme_transize_e eTransSize;     //!< Transfer element size. This is used to determine the group size
                                          //!< when transformation is applied.
} am_util_dme_wrapcpy_2d_config_t;

//*****************************************************************************
//
//! @brief Strided-copy configuration structure.
//
//*****************************************************************************
typedef struct
{
    uint32_t ui32SrcAddr;     //!< Address of the first source byte.
    uint32_t ui32DstAddr;     //!< Address of the first destination byte.
    uint32_t ui32SetSize;     //!< Number of bytes, M, to copy per set.
    uint32_t ui32SetCount;    //!< Number of sets, N, to copy.
    int32_t i32SrcStride;     //!< Source start-to-start signed stride in bytes.
    int32_t i32DstStride;     //!< Destination start-to-start signed stride in bytes.
    bool bInvertDst;          //!< When true, reverse each M-byte sequence in the destination.
} am_util_dme_strided_copy_config_t;

//*****************************************************************************
//
//! @brief Generate a memory copy configuration.
//!
//! @param psConfig - Pointer to the configuration structure to initialize.
//! @param ui32SrcAddr - Source address.
//! @param ui32DstAddr - Destination address.
//! @param ui32Length - Copy length in bytes.
//! @param bEnableLinkAddr - When true, also sets mask bits to load LINKATTR
//!                          and LINKADDR.
//!
//! @return AM_HAL_STATUS_SUCCESS when the configuration is written to
//!                               @p psConfig.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
uint32_t
am_util_dme_gencfg_memcpy(am_hal_dme_ch_config_t *psConfig,
                          uint32_t ui32SrcAddr,
                          uint32_t ui32DstAddr,
                          uint32_t ui32Length,
                          bool     bEnableLinkAddr);


//*****************************************************************************
//
//! @brief Generate a 2D memory copy configuration.
//!
//! @param psConfig - Pointer to the configuration structure to initialize.
//! @param psCopyConfig - Pointer to the copy configuration structure.
//! @param bEnableLinkAddr - When true, also sets mask bits to load LINKATTR
//!                          and LINKADDR.
//!
//! @return AM_HAL_STATUS_SUCCESS when the configuration is written to
//!                               @p psConfig.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         If the transformed source image exceeds the destination boundary,
//!         only the top-left region that fits is generated.
//
//*****************************************************************************
uint32_t
am_util_dme_gencfg_memcpy_2d(am_hal_dme_ch_config_t *psConfig,
                             am_util_dme_memcpy_2d_config_t *psCopyConfig,
                             bool bEnableLinkAddr);

//*****************************************************************************
//
//! @brief Generate a wrapped-copy configuration.
//!
//! @param psConfig - Pointer to the configuration structure to initialize.
//! @param psWrapCpyConfig - Pointer to the wrapped-copy configuration structure.
//! @param bEnableLinkAddr - When true, also sets mask bits to load LINKATTR
//!                          and LINKADDR.
//!
//! @return AM_HAL_STATUS_SUCCESS when the configuration is written to
//!                               @p psConfig.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
uint32_t
am_util_dme_gencfg_wrapcpy(am_hal_dme_ch_config_t *psConfig,
                           am_util_dme_1d_wrapcpy_config_t *psWrapCpyConfig,
                           bool bEnableLinkAddr);

//*****************************************************************************
//
//! @brief Generate a 2D wrapped-copy configuration.
//!
//! @param psConfig - Pointer to the configuration structure to initialize.
//! @param psWrapCpyConfig - Pointer to the 2D wrapped-copy configuration
//!                          structure.
//! @param bEnableLinkAddr - When true, also sets mask bits to load LINKATTR
//!                          and LINKADDR.
//!
//! @return AM_HAL_STATUS_SUCCESS when the configuration is written to
//!                               @p psConfig.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
uint32_t
am_util_dme_gencfg_wrapcpy_2d(am_hal_dme_ch_config_t *psConfig,
                              am_util_dme_wrapcpy_2d_config_t *psWrapCpyConfig,
                              bool bEnableLinkAddr);

//*****************************************************************************
//
//! @brief Generate a strided-copy configuration.
//!
//! @param psConfig - Pointer to the configuration structure to initialize.
//! @param psStridedCopyConfig - Pointer to the strided-copy configuration
//!                              structure.
//! @param bEnableLinkAddr - When true, also sets mask bits to load LINKATTR
//!                          and LINKADDR.
//!
//! @return AM_HAL_STATUS_SUCCESS when the configuration is written to
//!                               @p psConfig.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
uint32_t
am_util_dme_gencfg_strided_copy(am_hal_dme_ch_config_t *psConfig,
                                am_util_dme_strided_copy_config_t *psStridedCopyConfig,
                                bool bEnableLinkAddr);

//*****************************************************************************
//
//! @brief Generate a byte-swap configuration.
//!
//! @param psConfig - Pointer to the configuration structure to initialize.
//! @param ui32SrcAddr - Source address.
//! @param ui32DstAddr - Destination address.
//! @param ui32Length - Length in bytes.
//! @param ui32GroupSize - Group size of the byte-swap operation.
//! @param bEnableLinkAddr - When true, also sets mask bits to load LINKATTR
//!                          and LINKADDR.
//!
//! @return AM_HAL_STATUS_SUCCESS when the configuration is written to
//!                               @p psConfig.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//
//*****************************************************************************
uint32_t
am_util_dme_gencfg_byteswap(am_hal_dme_ch_config_t *psConfig,
                            uint32_t ui32SrcAddr,
                            uint32_t ui32DstAddr,
                            uint32_t ui32Length,
                            uint32_t ui32GroupSize,
                            bool bEnableLinkAddr);


//*****************************************************************************
//
//! @brief Copy a memory range.
//!
//! @param pToken - Open DME channel token.
//! @param ui32SrcAddr - Source address.
//! @param ui32DstAddr - Destination address.
//! @param ui32Length - Copy length in bytes.
//!
//! @return AM_HAL_STATUS_SUCCESS when the copy completes.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         Otherwise returns the status from the DME config/start/wait path.
//
//*****************************************************************************
extern uint32_t am_util_dme_memcpy(am_hal_dme_ch_access_token_t *pToken,
                                   uint32_t ui32SrcAddr,
                                   uint32_t ui32DstAddr,
                                   uint32_t ui32Length);


//*****************************************************************************
//
//! @brief Copy a 2D memory range.
//!
//! @param pToken - Open DME channel token.
//! @param psCopyConfig - Pointer to the copy configuration structure.
//!
//! @return AM_HAL_STATUS_SUCCESS when the copy completes.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         Otherwise returns the status from the DME config/start/wait path.
//
//*****************************************************************************
extern uint32_t am_util_dme_memcpy_2d(am_hal_dme_ch_access_token_t *pToken,
                                      am_util_dme_memcpy_2d_config_t *psCopyConfig);


//*****************************************************************************
//
//! @brief Wrapped 1D copy from source to destination range.
//!
//! @param pToken - Open DME channel token.
//! @param psWrapCpyConfig - Pointer to the wrapped-copy configuration structure.
//!
//! @return AM_HAL_STATUS_SUCCESS when the operation completes.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         Otherwise returns the status from the DME config/start/wait path.
//
//*****************************************************************************
extern uint32_t am_util_dme_wrapcpy(am_hal_dme_ch_access_token_t *pToken,
                                    am_util_dme_1d_wrapcpy_config_t *psWrapCpyConfig);

//*****************************************************************************
//
//! @brief Wrapped 2D copy from source to destination range.
//!
//! @param pToken - Open DME channel token.
//! @param psWrapCpyConfig - Pointer to the 2D wrapped-copy configuration
//!                          structure.
//!
//! @return AM_HAL_STATUS_SUCCESS when the operation completes.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         Otherwise returns the status from the DME config/start/wait path.
//
//*****************************************************************************
extern uint32_t am_util_dme_wrapcpy_2d(am_hal_dme_ch_access_token_t *pToken,
                                       am_util_dme_wrapcpy_2d_config_t *psWrapCpyConfig);

//*****************************************************************************
//
//! @brief Strided copy from source to destination.
//!
//! @param pToken - Open DME channel token.
//! @param psStridedCopyConfig - Pointer to the strided-copy configuration
//!                              structure.
//!
//! @return AM_HAL_STATUS_SUCCESS when the operation completes.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         Otherwise returns the status from the DME config/start/wait path.
//
//*****************************************************************************
extern uint32_t am_util_dme_strided_copy(am_hal_dme_ch_access_token_t *pToken,
                                         am_util_dme_strided_copy_config_t *psStridedCopyConfig);

//*****************************************************************************
//
//! @brief Byte-swap copy from source to destination buffer.
//!
//! @param pToken - Open DME channel token.
//! @param ui32SrcAddr - Source address.
//! @param ui32DstAddr - Destination address.
//! @param ui32Length - Length in bytes.
//! @param ui32GroupSize - Group size of the byte-swap operation.
//!
//! @return AM_HAL_STATUS_SUCCESS when the operation completes.
//!         AM_HAL_STATUS_INVALID_ARG if one or more arguments fail validation.
//!         Otherwise returns the status from the DME config/start/wait path.
//
//*****************************************************************************
extern uint32_t am_util_dme_byteswap(am_hal_dme_ch_access_token_t *pToken,
                                     uint32_t ui32SrcAddr,
                                     uint32_t ui32DstAddr,
                                     uint32_t ui32Length,
                                     uint32_t ui32GroupSize);

#ifdef __cplusplus
}
#endif

#endif // AM_UTIL_DME_H

//*****************************************************************************
//
//! @}
//
//*****************************************************************************
