/* Only declarations are needed: unused hardware functions are link-discarded. */
#ifndef TEST_HCI_DRV_APOLLO_H
#define TEST_HCI_DRV_APOLLO_H
#include "wsf_types.h"
#include "wsf_os.h"
uint32_t HciDrvRadioBoot(uint32_t);
void HciDrvHandler(wsfEventMask_t, wsfMsgHdr_t *);
void HciDrvHandlerInit(wsfHandlerId_t);
int HciVscSetRfPowerLevelEx(int);
#define TX_POWER_LEVEL_MINUS_20P0_dBm 0

typedef int txPowerLevel_t;

#endif
