/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#ifndef _MARSHALLING_H_
#define _MARSHALLING_H_

#include "VmOsal.h"
#include "lxcom_sock.h"
#include "minksocket.h"
#include "object.h"
#include "ObjectTable.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define MAX_OBJECT_COUNT        1024
#define MSG_BUFFER_PREALLOC     1024
// 4MB: Max page size in linux kernel
#define MSG_BUFFER_MAX          4*1024*1024

// To prevent size_t from underflow
#define FOR_ARGS(idxVar, counts, section)                              \
  for (int32_t idxVar = (int32_t)ObjectCounts_index##section(counts);  \
       idxVar < ((int32_t)ObjectCounts_index##section(counts)          \
               + (int32_t)ObjectCounts_num##section(counts));          \
       ++idxVar)

// To prevent size_t from underflow
#define CONTINUE_ARGS(idxVar, idxNow, counts, section)                 \
  for (int32_t idxVar = (int32_t)(idxNow);                             \
       idxVar < ((int32_t)ObjectCounts_index##section(counts)          \
               + (int32_t)ObjectCounts_num##section(counts));          \
       ++idxVar)

typedef union {
  lxcom_msg msg;
  lxcom_hdr hdr;
  uint8_t buf[MSG_BUFFER_PREALLOC];
  uint64_t _aligned_unused;
} FlatData;

/*  Marshalling workflow
 *
 *        Caller                Callee
 *  1. IFoo_invoke
 *  2. MarshalOut_caller
 *  3. Send request
 *                        4. Receive request
 *                        5. MarshalIn_callee
 *                        6. CFoo_invoke
 *                        7. MarshalOut_callee
 *                        8. Send response
 *  9. Receive response
 *  10. MarshalIn_caller
 *  11. Invocation done
 */
int32_t MarshalOut_caller(MinkSocket *minksock, ObjectTable *objTable, uint32_t invId,
                          int32_t handle, ObjectOp op, ObjectArg *args, ObjectCounts k,
                          FlatData **data, size_t *sizeData,
                          int *fds, size_t *numFds);

int32_t MarshalIn_callee(MinkSocket *minksock, ObjectTable *objTable,
                         FlatData *data, size_t sizeData,
                         int *fds, size_t numFds,
                         ObjectArg *args, void **bufBO, size_t sizeBufBO);

int32_t MarshalOut_callee(MinkSocket *minksock, ObjectTable *objTable,
                          uint32_t invId, ObjectArg *args, ObjectCounts k,
                          FlatData **data, size_t *sizeData,
                          int *fds, size_t *numFds);

int32_t MarshalIn_caller(MinkSocket *minksock, ObjectTable *objTable,
                         FlatData *data, size_t sizeData,
                         int *fds, size_t numFds,
                         int32_t k, ObjectArg *args);

void MarshalIn_calleeRelease(int32_t invokeCounts, ObjectArg *invokeArgs,
                             int *fds, int numFds);

void MarshalOut_calleeRelease(char *resvBuf, void *ptrResv,
                              Object targetObj, int32_t invokeCounts,
                              ObjectArg *invokeArgs, int32_t errInvoke);

void Marshal_perfEntryTag(MinkSocket *minksock, uint32_t ipcType, FlatData *Msg);

#if defined(__cplusplus)
}
#endif

#endif
