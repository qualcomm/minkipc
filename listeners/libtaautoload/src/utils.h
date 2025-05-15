// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>
#include <string.h>

#define MSGV printf
#define MSGD printf
#define MSGE printf

typedef struct {
  uint32_t timeLow;
  uint16_t timeMid;
  uint16_t timeHiAndVersion;
  uint8_t clockSeqAndNode[8];
} TEEC_UUID;

static inline int atomic_add(int* pn, int n) {
  return __sync_add_and_fetch(pn, n);  // GCC builtin
}

static inline size_t memscpy(void *dst, size_t dst_size,
                             const void  *src, size_t src_size)
{
  size_t  copy_size = (dst_size <= src_size) ? dst_size : src_size;
  memcpy(dst, src, copy_size);
  return copy_size;
}

#endif //__UTILS_H
