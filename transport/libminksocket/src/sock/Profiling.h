/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/

#ifndef __SOCK_PROFILE_H__
#define __SOCK_PROFILE_H__
#include "stdbool.h"
#include "Utils.h"
#include "VmOsal.h"

#ifndef OFFTARGET
#include "logging.h"
#else
#include "../include/logging.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

void Profiling_configProfile(void);
bool Profiling_getProfile(void);

/******************************************************************************/
/*************************  Log Macro Definitions  ****************************/
/******************************************************************************/
#define LOG_PERF(xx_fmt, ...) \
  do { \
    if (true == Profiling_getProfile()) { \
      uint64_t __current_time = vm_osal_getCurrentTimeUs(); \
      if (__current_time == 0) { \
        LOGP("PERF ERROR: %s:%u: Invalid time \n", __func__, __LINE__); \
      } else { \
        LOGP("PERF: (%5u:%-5u) %s:%u: timestamp(us)= %u%09u, " xx_fmt "", \
             vm_osal_getPid(), vm_osal_getTid(), __func__, __LINE__, \
             UINT64_HIGH(__current_time), UINT64_LOW(__current_time),\
             ##__VA_ARGS__); \
      } \
    } \
  } while(0)

#define LOG_PERF_OP(operation, xx_fmt, ...) \
 ({ \
    uint64_t duration = 0; \
    if (true == Profiling_getProfile()) { \
      uint64_t __start_time = UINT64_MAX, __end_time = 0; \
      __start_time = vm_osal_getCurrentTimeUs(); \
      operation; \
      __end_time = vm_osal_getCurrentTimeUs(); \
      if (__start_time == 0 || __end_time == 0) { \
        LOGP("PERF ERROR: %s:%u: Invalid time \n", __func__, __LINE__); \
      } else { \
        LOGP("PERF: (%5u:%-5u): %s:%u: take time(us)= %u%09u, " xx_fmt " \n", \
             vm_osal_getPid(), vm_osal_getTid(), __func__, __LINE__, \
             UINT64_HIGH(__end_time - __start_time), UINT64_LOW(__end_time - __start_time), \
             ##__VA_ARGS__); \
      } \
      duration = __end_time - __start_time; \
    } else { \
      operation; \
    } \
    duration; \
 })

#if defined(__cplusplus)
}
#endif

#endif /* __SOCK_PROFILE_H__ */

