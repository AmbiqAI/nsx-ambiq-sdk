//*****************************************************************************
//
//! @file hci_drv_510L_radio.h
//!
//! @brief Support functions for the Apollo510L radio.
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************
#ifndef HCI_DRV_510L_RADIO_H
#define HCI_DRV_510L_RADIO_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "am_devices_510L_radio.h"


#define LL_FEATURES_BYTE0  ( HCI_LE_SUP_FEAT_ENCRYPTION  \
                            | HCI_LE_SUP_FEAT_CONN_PARAM_REQ_PROC \
                            | HCI_LE_SUP_FEAT_EXT_REJECT_IND \
                            | HCI_LE_SUP_FEAT_SLV_INIT_FEAT_EXCH \
                            | HCI_LE_SUP_FEAT_LE_PING \
                            | HCI_LE_SUP_FEAT_DATA_LEN_EXT \
                            | HCI_LE_SUP_FEAT_PRIVACY \
                            | HCI_LE_SUP_FEAT_EXT_SCAN_FILT_POLICY )

#define LL_FEATURES_BYTE1  ( HCI_LE_SUP_FEAT_LE_2M_PHY \
                            |HCI_LE_SUP_FEAT_LE_CODED_PHY\
                            | HCI_LE_SUP_FEAT_LE_EXT_ADV \
                            | HCI_LE_SUP_FEAT_LE_PER_ADV \
                            | HCI_LE_SUP_FEAT_CH_SEL_2 )

#define LL_FEATURES_BYTE2  ( HCI_LE_SUP_FEAT_MIN_NUN_USED_CHAN )

#define LL_FEATURES_BYTE3  (HCI_LE_SUP_FEAT_PAST_SENDER \
                            | HCI_LE_SUP_FEAT_PAST_RECIPIENT \
                            | HCI_LE_SUP_FEAT_SCA_UPDATE \
                            |HCI_LE_SUP_FEAT_REMOTE_PUB_KEY_VALIDATION\
                            | HCI_LE_SUP_FEAT_CIS_MASTER \
                            | HCI_LE_SUP_FEAT_CIS_SLAVE \
                            | HCI_LE_SUP_FEAT_ISO_BROADCASTER \
                            | HCI_LE_SUP_FEAT_ISO_SYNC_RECEIVER )

#if (BT_53)
#define LL_FEATURES_BYTE4  (HCI_LE_SUP_FEAT_POWER_CONTROL_REQUEST \
                            | HCI_LE_SUP_FEAT_POWER_CHANGE_IND \
                            | HCI_LE_SUP_FEAT_PATH_LOSS_MONITOR \
                            | HCI_LE_SUP_FEAT_PRR_ADV_ADI \
                            | HCI_LE_SUP_FEAT_SUBRATING \
                            | HCI_LE_SUP_FEAT_SUBRATING_HOST_SUPPORT \
                            | HCI_LE_SUP_FEAT_CHAN_CLASSIFICATION )
#else
#define LL_FEATURES_BYTE4  (HCI_LE_SUP_FEAT_POWER_CONTROL_REQUEST \
                            | HCI_LE_SUP_FEAT_POWER_CHANGE_IND \
                            | HCI_LE_SUP_FEAT_PATH_LOSS_MONITOR \
                            | HCI_LE_SUP_FEAT_PRR_ADV_ADI \
                            | HCI_LE_SUP_FEAT_SUBRATING \
                            | HCI_LE_SUP_FEAT_CHAN_CLASSIFICATION )
#endif // BT_53

#if (BT_54)
#define LL_FEATURES_BYTE5  (HCI_LE_SUP_FEAT_ADV_CODING_SELECTION \
                            | HCI_LE_SUP_FEAT_ADV_CODING_SELECTION_HOST_SUPPORT \
                            | HCI_LE_SUP_FEAT_PER_ADV_WITH_RESP_ADVERTISER \
                            | HCI_LE_SUP_FEAT_PER_ADV_WITH_RESP_SCANNER )
#else
#define LL_FEATURES_BYTE5  (HCI_LE_SUP_FEAT_ADV_CODING_SELECTION \
                            | HCI_LE_SUP_FEAT_PER_ADV_WITH_RESP_ADVERTISER \
                            | HCI_LE_SUP_FEAT_PER_ADV_WITH_RESP_SCANNER )
#endif // BT_54

#define LL_FEATURES_BYTE6 (0ULL)

#define LL_FEATURES_BYTE7 (0ULL)

//*****************************************************************************
//
// AMBIQ vendor specific events
//
//*****************************************************************************
// Tx power level in dBm.
typedef enum
{
    TX_POWER_LEVEL_MINUS_20P0_dBm = -20,
    TX_POWER_LEVEL_MINUS_18P0_dBm = -18,
    TX_POWER_LEVEL_MINUS_16P0_dBm = -16,
    TX_POWER_LEVEL_MINUS_14P0_dBm = -14,
    TX_POWER_LEVEL_MINUS_12P0_dBm = -12,
    TX_POWER_LEVEL_MINUS_10P0_dBm = -10,
    TX_POWER_LEVEL_MINUS_8P0_dBm = -8,
    TX_POWER_LEVEL_MINUS_6P0_dBm = -6,
    TX_POWER_LEVEL_MINUS_4P0_dBm = -4,
    TX_POWER_LEVEL_MINUS_2P0_dBm = -2,
    TX_POWER_LEVEL_0P0_dBm = 0,
    TX_POWER_LEVEL_PLUS_1P0_dBm = 1,
    TX_POWER_LEVEL_PLUS_2P0_dBm = 2,
    TX_POWER_LEVEL_PLUS_3P0_dBm = 3,
    TX_POWER_LEVEL_PLUS_4P0_dBm = 4,
    TX_POWER_LEVEL_PLUS_5P0_dBm = 5,
    TX_POWER_LEVEL_PLUS_6P0_dBm = 6,
    TX_POWER_LEVEL_PLUS_7P0_dBm = 7,
    TX_POWER_LEVEL_PLUS_8P0_dBm = 8,
    TX_POWER_LEVEL_PLUS_9P0_dBm = 9,
    TX_POWER_LEVEL_PLUS_10P0_dBm = 10,
    TX_POWER_LEVEL_PLUS_11P0_dBm = 11,
    TX_POWER_LEVEL_PLUS_12P0_dBm = 12,
    TX_POWER_LEVEL_PLUS_13P0_dBm = 13,
    TX_POWER_LEVEL_INVALID,
}txPowerLevel_t;

typedef struct
{
    /// Connection handle
    uint16_t conhdl;
} hciVsGetConEvtCntCmd_t;

// Set the default BLE TX Output power to +0dBm.
#define TX_POWER_LEVEL_DEFAULT TX_POWER_LEVEL_0P0_dBm

//*****************************************************************************
//
// Hci driver functions
//
//*****************************************************************************
bool_t HciVscSetCustom_BDAddr(uint8_t *bd_addr);
extern void HciDrvHandler(wsfEventMask_t event, wsfMsgHdr_t *pMsg);
extern void HciDrvHandlerInit(wsfHandlerId_t handlerId);
extern void HciVscUpdateNvdsParam(void);
extern void HciVscUpdateBDAddress(void);
extern void HciVscUpdateLinklayerFeature(void);
extern void HciVscUpdateTxpwrLevel(txPowerLevel_t txPowerlevel);
extern void HciVsGetConEventCounter(uint16_t con_handle);
extern void HciVscGetDtmRssi(void);

#ifdef __cplusplus
}
#endif

#endif // HCI_DRV_510L_RADIO_H
