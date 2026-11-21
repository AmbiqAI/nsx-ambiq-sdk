/*************************************************************************************************/
/*!
 *  \file   wsf_msg.h
 *
 *  \brief  Message passing service.
 *
 *  Copyright (c) 2009-2018 Arm Ltd. All Rights Reserved.
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
#ifndef WSF_MSG_H
#define WSF_MSG_H

#include "wsf_queue.h"
#include "wsf_os.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Message buf structure */
typedef struct wsfMsg_tag
{
  struct wsfMsg_tag   *pNext;
  wsfHandlerId_t      handlerId;
} wsfMsg_t;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgDataAlloc
 *
 *  \brief  Allocate a data message buffer to be sent with WsfMsgSend().
 *
 *  \param  len       Message length in bytes.
 *  \param  tailroom  Tailroom length in bytes.
 *
 *  \return Pointer to data message buffer or NULL if allocation failed.
 */
/*************************************************************************************************/
void *WsfMsgDataAlloc(uint16_t len, uint8_t tailroom);

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgAlloc
 *
 *  \brief  Allocate a message buffer to be sent with WsfMsgSend().
 *
 *  \param  len   Message length in bytes.
 *
 *  \return Pointer to message buffer or NULL if allocation failed.
 */
/*************************************************************************************************/
void *WsfMsgAlloc(uint16_t len);

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgFree
 *
 *  \brief  Free a message buffer allocated with WsfMsgAlloc().
 *
 *  \param  pMsg  Pointer to message buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfMsgFree(void *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgSend
 *
 *  \brief  Send a message to an event handler.
 *
 *  \param  handlerId   Event handler ID.
 *  \param  pMsg        Pointer to message buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfMsgSend(wsfHandlerId_t handlerId, void *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgEnq
 *
 *  \brief  Enqueue a message.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  handerId  Set message handler ID to this value.
 *  \param  pElem     Pointer to message buffer.
 *
 *  \return None.
 */
/*************************************************************************************************/
void WsfMsgEnq(wsfQueue_t *pQueue, wsfHandlerId_t handlerId, void *pMsg);

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgDeq
 *
 *  \brief  Dequeue a message.
 *
 *  \param  pQueue      Pointer to queue.
 *  \param  pHandlerId  Handler ID of returned message; this is a return parameter.
 *
 *  \return Pointer to message that has been dequeued or NULL if queue is empty.
 */
/*************************************************************************************************/
void *WsfMsgDeq(wsfQueue_t *pQueue, wsfHandlerId_t *pHandlerId);

/*************************************************************************************************/
/*!
 *  \fn     WsfMsgPeek
 *
 *  \brief  Get the next message without removing it from the queue.
 *
 *  \param  pQueue      Pointer to queue.
 *  \param  pHandlerId  Handler ID of returned message; this is a return parameter.
 *
 *  \return Pointer to the next message on the queue or NULL if queue is empty.
 */
/*************************************************************************************************/
void *WsfMsgPeek(wsfQueue_t *pQueue, wsfHandlerId_t *pHandlerId);

// #### INTERNAL BEGIN ####
/*************************************************************************************************/
/*!
 *  \fn     WsfMsgInsert
 *
 *  \brief  Insert an message into a queue.
 *
 *  \param  pQueue    Pointer to queue.
 *  \param  handerId  Set message handler ID to this value.
 *  \param  pMsg      Pointer to message buffer.
 *  \param  pPrev     Pointer to previous message in the queue before message to be inserted.
 *                    Note:  set pPrev to NULL if pMsg is first message in queue.
 *  \return None.
 */
/*************************************************************************************************/
void WsfMsgInsert(wsfQueue_t *pQueue, wsfHandlerId_t handlerId,void *pMsg, void *pPrev);
// #### INTERNAL END ####
#ifdef __cplusplus
};
#endif

#endif /* WSF_MSG_H */
