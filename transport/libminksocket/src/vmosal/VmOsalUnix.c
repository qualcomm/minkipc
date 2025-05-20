/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#ifdef VNDR_UNIXSOCK
#include <sys/un.h>
#include <sys/socket.h>
#endif
#include "VmOsal.h"

void* vm_osal_malloc(ssize_t x)
{
  return malloc(x);
}

void vm_osal_free(void* x)
{
  return free(x);
}

void *vm_osal_zalloc(ssize_t x)
{
  void *ptemp = malloc(x);
  if (ptemp != NULL) {
    memset(ptemp, 0x0, x);
  }

  return ptemp;
}

void *vm_osal_calloc(ssize_t num, ssize_t size)
{
  void *ptemp = calloc(num, size);
  if (ptemp != NULL) {
    memset(ptemp, 0x0, (num*size));
  }

  return ptemp;
}

int vm_osal_cond_init(void* cond_object, void* attr)
{
  return pthread_cond_init((vm_osal_cond*)cond_object,
                           (const pthread_condattr_t *)attr);
}

int vm_osal_cond_deinit(void* cond_object)
{
  return pthread_cond_destroy((vm_osal_cond*)cond_object);
}

int vm_osal_mutex_lock(void* mutex_object)
{
  return pthread_mutex_lock((vm_osal_mutex*)mutex_object);
}

int vm_osal_mutex_init(void* mutex_object, void* attr)
{
  return pthread_mutex_init((vm_osal_mutex*)mutex_object,
                            (const pthread_mutexattr_t *)attr);
}

int vm_osal_mutex_deinit(void* mutex_object)
{
  return pthread_mutex_destroy((vm_osal_mutex*)mutex_object);
}

int vm_osal_mutex_unlock(void* mutex_object)
{
  return pthread_mutex_unlock((vm_osal_mutex*)mutex_object);
}

int vm_osal_cond_wait(void* cond_wait, void* mutex, int* condition)
{
  return pthread_cond_wait((vm_osal_cond *)cond_wait, (vm_osal_mutex *)mutex);
}

int vm_osal_cond_set(void* sig)
{
  return pthread_cond_signal((vm_osal_cond *)sig);
}

int vm_osal_cond_broadcast(void* sig)
{
  return pthread_cond_broadcast((vm_osal_cond *)sig);
}

struct vm_osal_thread_ctxt {
  void* (*fn)(void *);
  void *data;
};

void *vm_osal_thead_fn(void *data)
{
  struct vm_osal_thread_ctxt *ctxt = (struct vm_osal_thread_ctxt *)data;
  ctxt->fn(ctxt->data);
  free(ctxt);
  return 0;
}

int vm_osal_thread_create(vm_osal_thread *tid, void* (*fn)(void *data), void* data, char *thread_name)
{
  struct vm_osal_thread_ctxt *ctxt;

  ctxt = (struct vm_osal_thread_ctxt *)malloc(sizeof(*ctxt));
  if (!ctxt) {
    return -1;
  }

  ctxt->data = data;
  ctxt->fn = fn;

  return pthread_create(tid, NULL, vm_osal_thead_fn, ctxt);
}

int vm_osal_thread_detach(vm_osal_thread tid)
{
  return pthread_detach(tid);
}

vm_osal_thread vm_osal_thread_self(void)
{
  return pthread_self();
}

int vm_osal_thread_join(vm_osal_thread tid, void **retVal)
{
  return pthread_join(tid, retVal);
}

int vm_osal_create_TLS_key(vm_osal_key *key, void (*destructor)(void*))
{
  return pthread_key_create((vm_osal_key *)key, destructor);
}

int vm_osal_store_TLS_key(vm_osal_key key, const void *value)
{
  return pthread_setspecific(key, value);
}

void *vm_osal_retrieve_TLS_key(vm_osal_key key)
{
  return pthread_getspecific(key);
}

int vm_osal_socket_shutdown(int sockFd, int how)
{
  return shutdown(sockFd, how);
}

int vm_osal_socket_close(int sockFd)
{
  return close(sockFd);
}

int vm_osal_fd_close(int fd)
{
  return close(fd);
}

int vm_osal_mem_close(int memFd)
{
  return close(memFd);
}

int vm_osal_atomic_add(int* ptr, int number)
{
  return __sync_add_and_fetch(ptr, number);
}

int vm_osal_fd_dup(int oldfd)
{
  return dup(oldfd);
}

int vm_osal_poll(void* pollfds, int nfds, int timeout)
{
  return poll((struct pollfd *)pollfds, (nfds_t)nfds, timeout);
}

int vm_osal_pipe(int pipefd[2])
{
  return pipe(pipefd);
}

void vm_osal_sleep(unsigned int time_in_sec)
{
  sleep(time_in_sec);
  return;
}

void vm_osal_usleep(unsigned int time_in_microsec)
{
  usleep(time_in_microsec);
  return;
}

int vm_osal_getsockopt(int fd, int level, int optname,
                       void *optval, socklen_t *optlen)
{
#ifdef VNDR_UNIXSOCK
  return getsockopt(fd, level, optname, optval, optlen);
#else
  return -1;
#endif
}

vm_osal_pid vm_osal_getPid(void)
{
  return getpid();
}

vm_osal_pid vm_osal_getTid(void)
{
  return (vm_osal_pid)syscall(__NR_gettid);
}

uint64_t vm_osal_getCurrentTimeNs(void)
{
  struct timespec ts;
  if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
    perror("clock_gettime failed");
    return 0;
  }
  return (uint64_t)ts.tv_sec * 1000000000LLU + ts.tv_nsec;
}

uint64_t vm_osal_getCurrentTimeUs(void)
{
  struct timeval tv;
  if(gettimeofday(&tv, NULL) == -1) {
    perror("gettimeofday failed");
    return 0;
  }
  return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

int vm_osal_getTimeOfDay(vm_osal_timeval *tv, vm_osal_timezone *tz)
{
  return gettimeofday(tv, tz);
}

int vm_osal_rand_r(unsigned int *seedp)
{
  return rand_r(seedp);
}
