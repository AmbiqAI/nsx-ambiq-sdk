/*************************************************************************************************/
/*!
 *  \file   wsf_assert.c
 *
 *  \brief  Assert implementation.
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

#include "wsf_types.h"
#include "wsf_assert.h"
#include "wsf_os.h"

/*************************************************************************************************/
/*!
 *  \def    WsfAssert
 *
 *  \brief  Perform an assert action.
 *
 *  \param  pFile   Name of file originating assert.
 *  \param  line    Line number of assert statement.
 */
/*************************************************************************************************/
void WsfAssert(const char *pFile, uint16_t line)
{
  volatile uint8_t escape=0;

  /* spin forever if fatal error occurred */
  for(;;)
  {
    /*
     *  However, you can exit with a debugger by setting variable 'escape'.
     *  Handy to see where the assert happened if you cannot view the call stack.
    */
    if (escape)
    {
      break;
    }
  }
}
