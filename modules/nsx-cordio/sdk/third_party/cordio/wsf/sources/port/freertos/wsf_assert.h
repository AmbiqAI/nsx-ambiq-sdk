/*************************************************************************************************/
/*!
 *  \file   wsf_assert.h
 *
 *  \brief  Assert macro.
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
#ifndef WSF_ASSERT_H
#define WSF_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Function Prototypes
**************************************************************************************************/

void WsfAssert(const char *pFile, uint16_t line);

/**************************************************************************************************
  Macros
**************************************************************************************************/
#define WSF_ASSERT_ENABLED FALSE
/*************************************************************************************************/
/*!
 *  \def    WSF_ASSERT
 *        
 *  \brief  Run-time assert macro.  The assert executes when the expression is FALSE.
 *
 *  \param  expr    Boolean expression to be tested.
 */
/*************************************************************************************************/
#if WSF_ASSERT_ENABLED == TRUE
#define WSF_ASSERT(expr)      if (!(expr)) {WsfAssert(__FILE__, (uint16_t) __LINE__);}
#else
#define WSF_ASSERT(expr)
#endif

/*************************************************************************************************/
/*!
 *  \def    WSF_CT_ASSERT
 *        
 *  \brief  Compile-time assert macro.  This macro causes a compiler error when the
 *          expression is FALSE.  Note that this macro is generally used at file scope to
 *          test constant expressions.  Errors may result of it is used in executing code.
 *
 *  \param  expr    Boolean expression to be tested.
 */
/*************************************************************************************************/
#if WSF_ASSERT_ENABLED == TRUE
#define WSF_CT_ASSERT(expr)     extern char wsf_ct_assert[(expr) ? 1 : -1]
#else
// Fix -Wpedantic "ISO C does not allow extra ';' outside of a function" warning
#define WSF_CT_ASSERT(expr)     struct WsfNoCtAssert
#endif

#ifdef __cplusplus
};
#endif

#endif /* WSF_ASSERT_H */
