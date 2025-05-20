/********************************************************************
 Copyright (c) 2016, 2022-2024 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 *********************************************************************/
#include "Heap.h"
#include "ThreadPool.h"
#include "Utils.h"

struct  ThreadPool {
  int refs;
  vm_osal_thread aThreads[THREADPOOL_MAX_THREADS];
  vm_osal_thread bDelayCleaner;
  QList qlWork;
  vm_osal_mutex qMtx;
  vm_osal_cond qCnd;
  bool workForServer;
  bool bDone;
  int nThreads;
  int nIdleThreads;
};


static int QList_count(QList *pq)
{
  QNode *pn;
  int num = 0;

  QLIST_FOR_ALL(pq, pn) {
    ++num;
  }
  return num;
}

static void threadPool_cleanThreadRecord(ThreadPool *me, vm_osal_thread id) {
  for (int i = 0; i < THREADPOOL_MAX_THREADS; ++i) {
    if(id == me->aThreads[i]) {
      me->aThreads[i] = 0;
      --me->nThreads;
      return;
    }
  }
  return;
}

void ThreadPool_retain(ThreadPool *me)
{
  vm_osal_atomic_add(&me->refs, 1);
}

static void ThreadPool_delete(ThreadPool *me)
{
  if (NULL == me) {
    return;
  }

  vm_osal_mutex_deinit(&me->qMtx);
  vm_osal_cond_deinit(&me->qCnd);
  QNode *qn;
  QNode *qnn;
  QLIST_NEXTSAFE_FOR_ALL(&me->qlWork, qn, qnn) {
    QNode_dequeue(qn);
    free(qn);
  }

  LOG_TRACE("threadPool %p freed\n", me);
  HEAP_FREE_PTR(me);
}

void ThreadPool_stop(ThreadPool *me)
{
  if (!me->bDone) {
    vm_osal_mutex_lock(&me->qMtx);
    me->bDone = true;
    vm_osal_cond_broadcast(&me->qCnd);
    vm_osal_mutex_unlock(&me->qMtx);
    for (int i=0; i < THREADPOOL_MAX_THREADS; ++i) {
      if(0 != me->aThreads[i]) {
        if (me->workForServer) {
          vm_osal_thread_detach(me->aThreads[i]);
          LOG_TRACE("threadPool %p, threadID %ld detached\n", me, me->aThreads[i]);
        } else {
          if (me->aThreads[i] == vm_osal_thread_self()) {
            me->bDelayCleaner = me->aThreads[i];
            vm_osal_thread_detach(me->bDelayCleaner);
          } else {
            vm_osal_thread_join(me->aThreads[i], NULL);
            LOG_TRACE("threadPool %p, threadID %ld joined\n", me, me->aThreads[i]);
          }
        }
        me->aThreads[i] = 0;
        me->nThreads--;
      }
    }
  }
}

void ThreadPool_release(ThreadPool *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    if (0 == me->bDelayCleaner) {
      ThreadPool_delete(me);
    }
  }
}

static void *thread_entrypoint(void *arg)
{
  ThreadPool *me = (ThreadPool *)arg;
  ThreadWork *w = NULL;
  vm_osal_thread threadID = vm_osal_thread_self();
  LOG_TRACE("threadPool %p, threadID %ld running\n", me, threadID);
  while (!me->bDone) {
    vm_osal_mutex_lock(&me->qMtx);

    while (!me->bDone && QList_isEmpty(&me->qlWork)) {
      ++me->nIdleThreads;

      if (me->nIdleThreads > (QList_count(&me->qlWork) + THREADPOOL_MIN_IDLE_THREADS)) {
        --me->nIdleThreads;
        threadPool_cleanThreadRecord(me, threadID);
        vm_osal_mutex_unlock(&me->qMtx);
        if (me->workForServer) {
          ThreadPool_release(me);
        }
        vm_osal_thread_detach(threadID);
        return NULL;
      }

      vm_osal_cond_wait(&me->qCnd, &me->qMtx, NULL);
      --me->nIdleThreads;
    }

    if (me->bDone) {
      vm_osal_mutex_unlock(&me->qMtx);
      goto bail;
    }

    w = (ThreadWork *)QList_pop(&me->qlWork);
    if (w == NULL) {
      vm_osal_mutex_unlock(&me->qMtx);
      goto bail;
    }

    vm_osal_mutex_unlock(&me->qMtx);
    w->workFunc(w->args);
    free(w);
  }

bail:
  if (me->workForServer) {
    ThreadPool_release(me);
  } else if (me->bDelayCleaner == threadID) {
    ThreadPool_delete(me);
  }

  return NULL;
}

void ThreadPool_wait(ThreadPool *me)
{
  while (!QList_isEmpty(&me->qlWork)  ||
         me->nThreads != me->nIdleThreads) {
    vm_osal_sleep(0);
  }
}

static void ThreadPool_createThread(ThreadPool *me)
{
  int i = 0, j = THREADPOOL_MIN_IDLE_THREADS;

  while((i < THREADPOOL_MAX_THREADS) && (j > 0)) {
    if (me->aThreads[i] == 0) {
      vm_osal_thread_create(&me->aThreads[i], &thread_entrypoint, me, NULL);
      LOG_TRACE("threadPool %p construct threadID %ld\n", me, me->aThreads[i]);
      if (me->workForServer) {
        ThreadPool_retain(me);
      }
      ++me->nThreads;
      --j;
    }
    ++i;
  }

  return;
}

void ThreadPool_queue(ThreadPool *me, ThreadWork *work)
{
  vm_osal_mutex_lock(&me->qMtx);
  if (me->bDone) {
    vm_osal_cond_broadcast(&me->qCnd);
    vm_osal_mutex_unlock(&me->qMtx);
    free(work);
    return;
  }

  QList_appendNode(&me->qlWork, &work->n);
  LOG_TRACE("threadPool %p enqueue threadWork %p\n", me, work);

  if (QList_count(&me->qlWork) > me->nIdleThreads) {
    ThreadPool_createThread(me);
  }

  vm_osal_cond_broadcast(&me->qCnd);
  vm_osal_mutex_unlock(&me->qMtx);
}

ThreadPool *ThreadPool_new(bool workForServer)
{
  ThreadPool *me = HEAP_ZALLOC_TYPE(ThreadPool);
  if (me == NULL) {
    LOG_ERR("failed to allocate threadPool\n");
    return NULL;
  }

  if (0 != vm_osal_mutex_init(&me->qMtx, NULL)) {
    LOG_ERR("failed to init qMtx\n");
    goto bail;
  }

  if (0 != vm_osal_cond_init(&me->qCnd, NULL)) {
    LOG_ERR("failed to init qCnd\n");
    goto bail;
  }

  QList_construct(&me->qlWork);
  me->bDone = false;
  me->refs = 1;
  me->workForServer = workForServer;
  return me;

 bail:
  ThreadPool_release(me);
  return NULL;
}
