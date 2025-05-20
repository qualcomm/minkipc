/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/

#ifndef _TRANSPORT_UTILS_H_
#define _TRANSPORT_UTILS_H_

#include "VmOsal.h"

#ifndef OFFTARGET
#include "logging.h"
#else
#include "../include/logging.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

void Utils_configTrace(void);
bool Utils_getTrace(void);

#define TRUE_OR_CLEAN(expr, ...) \
  do {if (!(expr)) {LOG_ERR(__VA_ARGS__); goto cleanup;}} while (0)

#define POSITIVE_OR_CLEAN(expr, ...) \
  do {if ((expr) < 0) {LOG_ERR(__VA_ARGS__); goto cleanup;}} while (0)

#define SILENT_CHECK(expr) \
  do {if (!(expr)) {goto cleanup;}} while (0)

#ifndef UINT64_HIGH
#define UINT64_HIGH(value) (uint32_t)((value) / 1000000000)
#endif

#ifndef UINT64_LOW
#define UINT64_LOW(value)  (uint32_t)((value) % 1000000000)
#endif

#define LOG_TRACE(xx_fmt, ...) \
  do {                 \
    if (true == Utils_getTrace()) { \
      LOG("MinkIPC TRACE: (%5u:%-5u) %s:%u " xx_fmt "", \
          vm_osal_getPid(), vm_osal_getTid(), __func__, __LINE__, ##__VA_ARGS__); \
    } \
  } while (0);

#define LOG_MSG(xx_fmt, ...) \
  LOG("MinkIPC MESSAGE: (%5u:%-5u) %s:%u " xx_fmt "", vm_osal_getPid(), \
       vm_osal_getTid(), __func__, __LINE__, ##__VA_ARGS__)

#define LOG_ERR(xx_fmt, ...) \
  LOGE("MinkIPC ERROR: (%5u:%-5u) %s:%u " xx_fmt "", vm_osal_getPid(), \
       vm_osal_getTid(), __func__, __LINE__, ##__VA_ARGS__)

#if defined(__cplusplus)
}
#endif

#endif /* _TRANSPORT_UTILS_H_ */
