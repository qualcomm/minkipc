// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __DMAMEMPOOL_H_
#define __DMAMEMPOOL_H_

#include <stddef.h>
#include <object.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALIGN_PAGESIZE(LEN) (((LEN) + (getpagesize() -1)) & ~((getpagesize() -1)))

/**
 *  @brief Error Codes
 *
 *  MEM_OP_SUCCESS:     Success
 *  MEM_ALLOC_FAILED:   DMA Memory Allocation Failed
 *  MEM_RELEASE_FAILED: DMA Memory Released Failed
 */
#define INVALID_FD -1
#define MEM_OP_SUCCESS      Object_OK
#define MEM_ERROR_CODE(VAL) (Object_ERROR_USERBASE + (VAL))
#define MEM_ALLOC_FAILED    MEM_ERROR_CODE(0)
#define MEM_RELEASE_FAILED  MEM_ERROR_CODE(1)

/**
 * @brief struct MemoryBuffer
 * @members:
 *           allocatorInit: Init flag in case allocator is already Created
 *           memObj: Memory object representing the MemoryBuffer
 *           memBuf: Memory Buffer
 *           buffer_len : length of Buffer
 */
struct MemoryBuffer
{
  bool allocatorInit;
  Object memObj;
  void * memBuf;
  size_t bufferLen;
  MemoryBuffer();
  ~MemoryBuffer();
};

/**
 * @brief Initialize a DMA buffer for sharing with QTEE.
 *
 * @param memoryPtr Pointer to Memory structure.
 * @param buffLen Length of the DMA buffer to initialize.
 * @param rootObj A root object for initiating communication with QTEE.
 * @return MEM_OP_SUCCESS on success.
 *         MEM_ALLOC_FAILED on failure.
 */
int DMAMemPoolGetBuff(struct MemoryBuffer *memoryPtr, size_t buffLen, Object rootObj);

/**
 * @brief Release the DMA buffer shared with QTEE.
 *
 * @param memoryPtr Pointer to Memory structure.
 * @return MEM_OP_SUCCESS on success.
 *         MEM_RELEASE_FAILED on failure.
 */
int DMAMemPoolReleaseBuff(struct MemoryBuffer *memoryPtr);

#ifdef __cplusplus
}
#endif

#endif //__DMAMEMPOOL_H_
