//*****************************************************************************
//
//! @file dm_ead.h
//!
//! @brief Device manager encrypted advertising data module interface
//
//*****************************************************************************

//*****************************************************************************
//
// ${copyright}
//
// This is part of revision ${version} of the AmbiqSuite Development Package.
//
//*****************************************************************************

#ifndef DM_EAD_H
#define DM_EAD_H

#include "wsf_os.h"
#include "smp_api.h"
#include "dm_main.h"

#ifdef __cplusplus
extern "C" {
#endif

#if (BT_54)
/**************************************************************************************************
  Macros
**************************************************************************************************/

/* DM sec event handler messages */
enum
{
  DM_EAD_MSG_API_ENC_CNF = DM_MSG_START(DM_ID_EAD),
  DM_EAD_MSG_API_DEC_CNF,
};


/**************************************************************************************************
  Data types
**************************************************************************************************/

/* Data type for DM_EAD_MSG_API_ENC_CNF */
typedef struct
{
  secCcmEncMsg_t        msg;
} dmEadEncrypt_t;

/* Data type for DM_EAD_MSG_API_DEC_CNF */
typedef struct
{
  secCcmDecMsg_t        msg;
} dmEadDecrypt_t;

typedef union
{
  wsfMsgHdr_t           hdr;
  dmEadEncrypt_t        eadEnc;
  dmEadDecrypt_t        eadDec;
} dmEadMsg_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/**************************************************************************************************
  Function declarations
**************************************************************************************************/

/* component interface */
void dmEadMsgHandler(dmEadMsg_t *pMsg);

#endif // BT_54

#ifdef __cplusplus
};
#endif

#endif /* DM_EAD_H */
