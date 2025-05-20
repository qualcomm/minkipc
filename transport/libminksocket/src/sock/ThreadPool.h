/********************************************************************
 Copyright (c) 2016, 2022-2024 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 *********************************************************************/
#ifndef __THREADPOOL_H
#define __THREADPOOL_H

#include "VmOsal.h"
#include "qlist.h"

#define THREADPOOL_MIN_IDLE_THREADS 4
#define THREADPOOL_MAX_THREADS 50

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ThreadPool ThreadPool;

typedef void *(*ThreadWorkFunc)(void *);

typedef struct ThreadWork {
   QNode n;
   ThreadWorkFunc workFunc;
   void *args;
} ThreadWork;

static inline void ThreadWork_init(ThreadWork *w,
                                   ThreadWorkFunc func,
                                   void *args) {
  QNode_construct(&w->n);
  w->workFunc = func;
  w->args = args;
}

ThreadPool *ThreadPool_new(bool workForServer);
void ThreadPool_retain(ThreadPool *me);
void ThreadPool_release(ThreadPool *me);

//wait for all threads to exit gracefully
void ThreadPool_wait(ThreadPool *me);
void ThreadPool_stop(ThreadPool *me);
void ThreadPool_queue(ThreadPool *me, ThreadWork *work);

#ifdef __cplusplus
}
#endif
#endif /* __THREADPOOL_H */
