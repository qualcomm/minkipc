/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

// simheap.h: Heap functions specific to the "sim" platform.
//
// The 'sim' implementation of heap offers some additional capabilities to
// benefit unit testing.
//
// When compiled with `-D_DEBUG`, allocations will be tracked and tagged
// with the location of the allocation. Also, each `heap_free` detects
// pointers that do not point to a currently allocated node, and nodes that
// are freed from from a protection domain other than the one used to
// allocate them.

#ifndef __SIMHEAP_H
#define __SIMHEAP_H

#include <stddef.h>  // size_t
#include <stdint.h>  // SIZE_MAX

#if defined (__cplusplus)
extern "C" {
#endif


#define SIMHEAP_PLACEARG        (__FILE__ ":" SIMHEAP_QUOTE(__LINE__))
#define SIMHEAP_QUOTEV(v)       #v
#define SIMHEAP_QUOTE(sym)      SIMHEAP_QUOTEV(sym)

#ifdef _DEBUG
#  define simheap_zalloc(s)       simheap__zalloc((s), SIMHEAP_PLACEARG)
#  define simheap_free(p)         simheap__free((p), SIMHEAP_PLACEARG)
#  define simheap_memdup(p,s)     simheap__memdup((p), (s), SIMHEAP_PLACEARG)
#else
   void *simheap_zalloc(size_t size);
   void  simheap_free(void *ptr);
   void *simheap_memdup(const void *ptr, size_t size);
#endif

void * simheap__zalloc(size_t size, const char *place);
void * simheap__memdup(const void *ptr, size_t size, const char *place);
void   simheap__free(void *ptr, const char *place);
size_t simheap_umul(size_t a, size_t b);


static inline void *simheap_calloc(size_t num, size_t size)
{
  if (num > SIZE_MAX/size) {
    return NULL;
  }
  return simheap_zalloc(num * size);
}


// Count the total number of heap nodes allocated.
//
int simheap_count(void);


// If more than `countExpected` nodes are allocated, display the line and
// file that allocated the most recent nodes.
//
void simheap__checkLeaks(int countExpected, const char *place);

#define simheap_checkLeaks(cnt)   simheap__checkLeaks((cnt), SIMHEAP_PLACEARG)


// Free nodes that were allocated within a particular process.
//
void simheap_freeGroup(void *proc);


// Mark heap nodes with a group context pointer
//
void simheap_markGroups(void *(*getGroup)(void));

// Cause allocation to fail in the future.
//   when==0 => don't fail
//   when==N => fail on Nth alloc (1 == very next alloc)
//   howMany == how many consecutive failures (0 => keep failing)
//
void simheap_failAt(int when, int howMany);


#if defined (__cplusplus)
}
#endif

#endif // __SIMHEAP_H
