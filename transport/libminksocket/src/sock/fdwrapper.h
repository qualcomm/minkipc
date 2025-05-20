/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#ifndef __FDWRAPPER_H
#define __FDWRAPPER_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "object.h"
#include "qlist.h"
#include "VmOsal.h"

typedef struct FdWrapper {
  QNode node;
  int refs;
  int handle;
  int descriptor;
  Object dependency;
  bool needToCloseFd;
  void *virtAddr;
  bool mapped;
  size_t bufSize;
  unsigned int ipcFlags;
  bool fromMinkMem;
} FdWrapper;

Object FdWrapper_new(int fd);
FdWrapper *FdWrapperFromObject(Object obj);

bool isWrappedFd(Object obj, int* fd);

FdWrapper *FdWrapper_newInternal(int fd, bool needToCloseFd, void *ptr,
                                 size_t bufSize, bool mapped, bool fromMinkMem,
                                 unsigned int ipcFlags);

Object FdWrapperToObject(FdWrapper *context);

Object FdWrapper_newWithCloseFlags(int fd, bool needToCloseFd);

#if defined (__cplusplus)
}
#endif

#endif // __FdWrapper_H
