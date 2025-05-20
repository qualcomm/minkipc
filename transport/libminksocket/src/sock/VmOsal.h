/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/

#ifndef _VM_OSAL_H_
#define _VM_OSAL_H_

#ifdef _CUSTOMIZED_PLATFORM
#else // _CUSTOMIZED_PLATFORM
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#endif //_CUSTOMIZED_PLATFORM

#if defined (__cplusplus)
extern "C" {
#endif /* __cplusplus */

#ifdef _CUSTOMIZED_PLATFORM
typedef struct mutex vm_osal_mutex;
typedef struct wait_queue_head vm_osal_cond;
typedef struct task_struct *vm_osal_thread;
typedef struct pollfd vm_osal_pollfd;
typedef ssize_t  vm_osal_key;
typedef int vm_osal_pid;
typedef struct {long tv_sec; long tv_usec;} vm_osal_timeval;
typedef struct {int tz_minuteswest; int tz_dsttime;} vm_osal_timezone;
#else // _CUSTOMIZED_PLATFORM
typedef struct pollfd vm_osal_pollfd;
typedef pthread_mutex_t vm_osal_mutex;
typedef pthread_cond_t vm_osal_cond;
typedef pthread_t vm_osal_thread;
typedef pthread_key_t  vm_osal_key;
typedef pid_t vm_osal_pid;
typedef struct timeval vm_osal_timeval;
typedef struct timezone vm_osal_timezone;
long syscall(long number, ...);
#endif // _CUSTOMIZED_PLATFORM

/** APIs exposed by OSAL Layer */
/* Mutex related OS agnostic APIs*/
int vm_osal_mutex_init(void *mutex_object, void *attr);
int vm_osal_mutex_lock(void *mutex_object);
int vm_osal_mutex_unlock(void *mutex_object);
int vm_osal_mutex_deinit(void *mutex_object);

/* Sync/cond wait OS agnostic APIs*/
int vm_osal_cond_wait(void* cond_wait, void* mutex, int* condition);
int vm_osal_cond_init(void *signal_object, void *attr);
int vm_osal_cond_set(void *sig);
int vm_osal_cond_broadcast(void* sig);
int vm_osal_cond_deinit(void *signal_object);

/* Thread related OS agnostic APIs*/
int vm_osal_thread_create(vm_osal_thread *tid,void* (*fn)(void *data),
                          void* data, char *thread_name);
int vm_osal_thread_detach(vm_osal_thread tid);
vm_osal_thread vm_osal_thread_self(void);
int vm_osal_thread_join(vm_osal_thread tid, void **retVal);

/* Thread related data manipulation  - OS agnostic APIs*/
int vm_osal_create_TLS_key(vm_osal_key *key, void (*destructor)(void*));
int vm_osal_store_TLS_key(vm_osal_key key, const void *value);
void *vm_osal_retrieve_TLS_key(vm_osal_key key);

/* Misc */
int vm_osal_socket_shutdown(int sockFd, int how);
int vm_osal_socket_close(int sockFd);
int vm_osal_fd_close(int fd);
int vm_osal_mem_close(int memFd);
int vm_osal_atomic_add(int *ptr, int number);
int vm_osal_fd_dup(int oldfd);
int vm_osal_poll(void* pollfds, int nfds, int timeout);
int vm_osal_pipe(int pipefd[2]);
void vm_osal_sleep(unsigned int time_in_sec);
void vm_osal_usleep(unsigned int time_in_microsec);
int vm_osal_getsockopt(int fd, int level, int optname,
                       void *optval, socklen_t *optlen);
vm_osal_pid vm_osal_getPid(void);
vm_osal_pid vm_osal_getTid(void);

uint64_t vm_osal_getCurrentTimeNs(void);
uint64_t vm_osal_getCurrentTimeUs(void);
int vm_osal_getTimeOfDay(vm_osal_timeval *tv, vm_osal_timezone *tz);
int vm_osal_rand_r(unsigned int *seedp);
size_t vm_osal_getDMAFdSize(int dmaBufFd);

#if defined (__cplusplus)
}
#endif /* __cplusplus */

#endif //_VM_OSAL_H_
