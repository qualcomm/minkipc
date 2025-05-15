// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <cstdbool>
#include <memory>
#include <map>
#include <sys/mman.h>
#include <vector>

#include "dmaMemPool.h"
#include "MinkCom.h"
#include "taImageReader.h"
#include "utils.h"

/* @constructor */
MemoryBuffer::MemoryBuffer():memObj(Object_NULL), memBuf(nullptr)
{
  allocatorInit = false;
  bufferLen = 0U;
}

/* @destructor */
MemoryBuffer::~MemoryBuffer()
{
  allocatorInit = false;
  Object_ASSIGN_NULL(memObj);
}

int DMAMemPoolGetBuff(struct MemoryBuffer *memoryPtr, size_t buffLen, Object rootObj)
{
  int retVal = MEM_ALLOC_FAILED;
  int32_t rv = Object_OK;

  /* Already Initialized? */
  if(true == memoryPtr->allocatorInit)
  {
      return MEM_OP_SUCCESS;
  }

  memoryPtr->bufferLen = ALIGN_PAGESIZE(buffLen);

  rv = MinkCom_getMemoryObject(rootObj, memoryPtr->bufferLen, &memoryPtr->memObj);
  if (Object_isERROR(rv)) {
      MSGE("Failed to obtain memory object: 0x%x", rv);
      goto ERROR_HANDLE;
  }

  rv = MinkCom_getMemoryObjectInfo(memoryPtr->memObj, &memoryPtr->memBuf, &memoryPtr->bufferLen);
  if (Object_isERROR(rv)) {
      Object_ASSIGN_NULL(memoryPtr->memObj);
      MSGE("getMemoryObjectInfo failed: 0x%x\n", rv);
      goto ERROR_HANDLE;
  }

  memoryPtr->allocatorInit = true;

  /* Success */
  return MEM_OP_SUCCESS;

ERROR_HANDLE:
  memoryPtr->memBuf =(unsigned char *)MAP_FAILED;
  memoryPtr->bufferLen = 0U;

  return retVal;
}

int DMAMemPoolReleaseBuff(struct MemoryBuffer *memoryPtr)
{
  int retVal = MEM_RELEASE_FAILED;
  if((false == memoryPtr->allocatorInit) || (MAP_FAILED == memoryPtr->memBuf))
  {
    MSGE("DMA Allocator not initialized or mapped");
    goto ERROR_HANDLE;
  }

  memoryPtr->bufferLen = 0;
  retVal = MEM_OP_SUCCESS;

ERROR_HANDLE:
  delete(memoryPtr);
  memoryPtr = nullptr;

  return retVal;
}
