/********************************************************************
 Copyright (c) 2016-2017 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
*********************************************************************/
#ifndef __HEAP_PORT_H
#define __HEAP_PORT_H

#include <stdlib.h>
#include <string.h>

#if defined (__cplusplus)
extern "C" {
#endif
static inline void *zalloc(size_t size) {
  void *ptr = malloc(size);
  if (ptr) {
    memset(ptr, '\0', size);
  }
  return ptr;
}

#define heap_zalloc(x)          zalloc(x)
#define heap_calloc(num, siz)   calloc(num, siz)
#define heap_free(x)            (free(x), x = NULL)
#define heap_memdup(ptr, siz)   memdup(ptr, siz)

#if defined (__cplusplus)
}
#endif

#endif // __HEAP_PORT_H

