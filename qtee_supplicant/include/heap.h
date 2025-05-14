// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __HEAP_H
#define __HEAP_H

/**
 * heap_port.h supplies definitions compatible with the following:
 *
 *   void *heap_zalloc(size_t size);
 *   void *heap_calloc(size_t num, size_t size);
 *   void  heap_free(void *ptr);
 *   void *heap_memdup(const void *ptr, size_t size);
 */
#include "heap_port.h"

#define HEAP_ZALLOC_TYPE(ty)      ((ty *) heap_zalloc(sizeof(ty)))

#define HEAP_ZALLOC_ARRAY(ty, k)  ((ty *) heap_calloc((k), sizeof(ty)))

#define HEAP_FREE_PTR(var)        ((void) (heap_free(var), (var) = 0))

// Older convention:
#define HEAP_ZALLOC_REC(ty)       HEAP_ZALLOC_TYPE(ty)

#endif // __HEAP_H
