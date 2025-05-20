/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>  // SIZE_MAX
#include <limits.h>  // LONG_MAX
#include <stdio.h>
#include "Heap.h"
#include "qtest.h"
#include "simheap.h"

#ifdef VERBOSE
#  define KLOG(...)  printf(__VA_ARGS__)
#else
#  define KLOG(...)
#endif

#define COOKIE_SIZE  sizeof(long)

#define COOKIE  (LONG_MAX / 11777) * 7937


typedef struct Node Node;
struct Node {
   Node *next;
   void *ptr;
   size_t size;
   const char *place;
   void *proc;
};


static Node *head = 0;

static int g_successCount = 0;
static int g_failureCount = 0;

static void *nullGetGroup(void)
{
  return NULL;
}

static void *(*g_getGroup)(void) = nullGetGroup;


static void mark(void *ptr, size_t size, const char *place)
{
   Node *pn = (Node *)malloc(sizeof(Node));

   long cookie = COOKIE;
   memcpy((char*)ptr + size, &cookie, COOKIE_SIZE);

   qt_assert(pn);
   pn->next = head;
   pn->ptr = ptr;
   pn->size = size;
   pn->place = place;
   pn->proc = g_getGroup();
   head = pn;
}


static void unmark(void *ptr, const char *place)
{
   Node *p, **pp;
   for (pp = &head; (p = *pp) != NULL; pp = &p->next) {
      if (p->ptr == ptr) {
         *pp = p->next;
         break;
      }
   }

   if (!p) {
     // free(NULL) valid in standard C
     return;
   }

   if (p->proc != g_getGroup()) {
      printf("%s: free of %p from wrong process %p!\n", place, ptr, g_getGroup());
      printf("%s: ... allocated here in process %p!\n", p->place, p->proc);
      exit(1);
   }

   long cookie;
   memcpy(&cookie, (char*)p->ptr + p->size, sizeof(cookie));

   if (cookie != COOKIE) {
      printf("%s: write past end of heap node %p\n", place, ptr);
      printf("%s: allocated here in process %p\n", p->place, p->proc);
      exit(1);
   }

   memset(p->ptr, 0xFE, p->size);

   free(p);
   return;
}


int simheap_count(void)
{
   int cnt = 0;

   for (Node *p = head; p != NULL; p = p->next) {
      ++cnt;
   }

   return cnt;
}


void simheap__checkLeaks(int countExpected, const char *place)
{
   int cntWarn = simheap_count() - countExpected;

   if (cntWarn <= 0) {
      qt_eqi(cntWarn, 0);
      return;
   }

   printf("%s: detected %d leak%s!\n", place, cntWarn, (cntWarn == 1 ? "" : "s"));
   for (Node *p = head; p != NULL; p = p->next) {
      if (--cntWarn >= 0) {
        printf("%s: allocated %p[%u] [proc=%p]\n",
               p->place, p->ptr, (unsigned) p->size, p->proc);
      }
   }
   exit(1);
}


// Mark heap nodes with a group context pointer
//
void simheap_markGroups(void *(*getGroup)(void))
{
  g_getGroup = getGroup ? getGroup : nullGetGroup;
}


// Free nodes that were allocated within a particular process.
//
void simheap_freeGroup(void *proc)
{
   Node *p, **pp;
   for (pp = &head; (p = *pp) != NULL;) {
      KLOG("free %p: %s%p.proc = %p\n", proc, p->place, p->ptr, p->proc);
      if (p->proc == proc) {
         *pp = p->next;
         free(p);
      } else {
         pp = &p->next;
      }
   }
}

void *simheap__zalloc(size_t size, const char *place)
{
   if (g_successCount && --g_successCount == 0) {
      KLOG("%s: zalloc NULL...\n", place);
      if (--g_failureCount) {
         g_successCount = 1; // keep failing
      }
      return NULL;
   }

   void *ptr = malloc(size + COOKIE_SIZE);
   if (ptr) {
      memset(ptr, '\0', size);
      mark(ptr, size, place);
   }
   KLOG("%s: zalloc [%p]\n", place, ptr);

   return ptr;
}


void simheap__free(void *ptr, const char *place)
{
   unmark(ptr, place);
   free(ptr);
}


size_t simheap_umul(size_t a, size_t b)
{
   if (b > 0 && a > SIZE_MAX / b) {
      return SIZE_MAX;
   }
   return a*b;
}


void simheap_failAt(int when, int howMany)
{
   g_successCount = when;
   g_failureCount = howMany;
}

void *simheap__memdup(const void *ptrIn, size_t sizeIn, const char *place)
{
  void *ptr = simheap__zalloc(sizeIn, "?");
  if (ptr) {
    memcpy(ptr,ptrIn, sizeIn);
  }
  return ptr;
}

#if !defined(_DEBUG)

void *simheap_zalloc(size_t size)
{
   return simheap__zalloc(size, "?");
}

void simheap_free(void *ptr)
{
   simheap__free(ptr, "?");
}

void *simheap_memdup(const void *ptrIn, size_t sizeIn)
{
   return simheap__memdup(ptrIn, sizeIn, "?");
}

#endif
