// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CREGISTER_TABUFF_H_
#define __CREGISTER_TABUFF_H_

#include <list>
#include <object.h>
#include <stdint.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

/* TA binaries path on device */
#define TA_PATH_LIST_SIZE 1
static const std::string ta_path_list[TA_PATH_LIST_SIZE] = { "/data" };

typedef struct
{
  std::list<std::string> searchLocations;
  Object rootObj;
  int refs;
} CRequestTABuffer;

/**
 * @brief Create a callback object for loading TA.
 *
 * @param RequestTABufferObj_ptr Buffer returned to client.
 * @param rootObj A root object for initiating communication with QTEE.
 * @return Object_OK on success;
 *         Object_ERROR on failure;
 */
int32_t CRequestTABuffer_open(Object *RequestTABufferObj_ptr, Object rootObj);

/**
 * @brief Return a memory object representing a buffer with the TA requested
 *        by QTEE.
 *
 * @param me The local object associated with this interface.
 * @param AppUID UID for requested TA.
 * @param appElf Output memory object representing the buffer with TA Image.
 * @return Object_OK on success;
 *         Object_ERROR on failure;
 */
int32_t CRequestTABuffer_get(CRequestTABuffer *me, const void *uuid_ptr, size_t uuid_len,
			     Object *RequestTABufferObj_ptr);

#ifdef __cplusplus
}
#endif

#endif //__CREGISTER_TABUFF_H_
