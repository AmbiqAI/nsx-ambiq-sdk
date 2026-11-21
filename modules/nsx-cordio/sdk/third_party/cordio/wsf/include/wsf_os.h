/*************************************************************************************************/
/*!
 *  \file   wsf_os.h
 *
 *  \brief  Software foundation OS API.
 *
 *  Copyright (c) 2009-2019 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2020 Packetcraft, Inc.
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/
#ifndef WSF_OS_H
#define WSF_OS_H

#include "wsf_os_int.h"
#include "wsf_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Configuration
**************************************************************************************************/

/* OS Diagnistics */
#ifndef WSF_OS_DIAG
#define WSF_OS_DIAG                             FALSE
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* Invalid Task Identifier */
#define WSF_INVALID_TASK_ID                     0xFF

#if WSF_OS_DIAG == TRUE
#define WSF_OS_GET_ACTIVE_HANDLER_ID()          WsfActiveHandler
#else
#define WSF_OS_GET_ACTIVE_HANDLER_ID()          WSF_INVALID_TASK_ID
#endif /* WSF_OS_DIAG */

/**************************************************************************************************
  External Variables
**************************************************************************************************/

/* Diagnistic Task Identifier */
extern wsfHandlerId_t WsfActiveHandler;

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/*! Common message structure passed to event handler */
typedef struct
{
  uint16_t        param;          /*! General purpose parameter passed to event handler */
  uint8_t         event;          /*! General purpose event value passed to event handler */
  uint8_t         status;         /*! General purpose status value passed to event handler */
} wsfMsgHdr_t;

/**************************************************************************************************
  Callback Function Types
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     wsfEventHandler_t
 *        
 *  \brief  Event handler callback function.
 *
 *  \param  event    Mask of events set for the event handler.
 *  \param  pMsg     Pointer to message for the event handler.
 *
 *  \return None.
 */
/*************************************************************************************************/
typedef void (*wsfEventHandler_t)(wsfEventMask_t event, wsfMsgHdr_t *pMsg);

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     WsfSetEvent
 *        
 *  \brief  Set an event for an event handler.
 *
 *  \param  handlerId   Handler ID.
 *  \param  event       Event or events to set.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfSetEvent(wsfHandlerId_t handlerId, wsfEventMask_t event);

/*************************************************************************************************/
/*!
 *  \fn     WsfTaskLock
 *        
 *  \brief  Lock task scheduling.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTaskLock(void);

/*************************************************************************************************/
/*!
 *  \fn     WsfTaskUnlock
 *        
 *  \brief  Unlock task scheduling.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTaskUnlock(void);

/*************************************************************************************************/
/*!
 *  \fn     WsfTaskSetReady
 *        
 *  \brief  Set the task used by the given handler as ready to run.
 *
 *  \param  handlerId   Event handler ID.
 *  \param  event       Task event mask.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfTaskSetReady(wsfHandlerId_t handlerId, wsfTaskEvent_t event);

/*************************************************************************************************/
/*!
 *  \fn     WsfTaskMsgQueue
 *        
 *  \brief  Return the task message queue used by the given handler.
 *
 *  \param  handlerId   Event handler ID.
 *
 *  \return Task message queue.
 */
/*************************************************************************************************/
wsfQueue_t *WsfTaskMsgQueue(wsfHandlerId_t handlerId);

/*************************************************************************************************/
/*!
 *  \fn     WsfOsSetNextHandler
 *        
 *  \brief  Set the next WSF handler function in the WSF OS handler array.  This function
 *          should only be called as part of the OS initialization procedure.
 *
 *  \param  handler    WSF handler function.
 *
 *  \return WSF handler ID for this handler.
 */
/*************************************************************************************************/
wsfHandlerId_t WsfOsSetNextHandler(wsfEventHandler_t handler);

/*************************************************************************************************/
/*!
 *  \fn     wsfOsSetEventObject
 *        
 *  \brief  Pass RTOS event object to wsf OS scheduler. This function
 *          should only be called as part of the OS (RTOS) initialization procedure.
 *
 *  \param  event_object    The pointer to an event object for RTOS.
 *
 *  \return None
 */
/*************************************************************************************************/
void wsfOsSetEventObject(void *event_object);


#ifdef __cplusplus
};
#endif

#endif /* WSF_OS_H */
