// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/* Copy of libminkadaptor/src/supplicant.c; see supplicant.h for why. Keep the
 * two in sync; they are byte identical apart from this comment.
 */

#include <linux/tee.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ucontext.h>
#include <unistd.h>

#include "supplicant.h"

/* Per thread kill-signal pending flag */
static __thread uint32_t sig_pending = 0;

/* Defined in syscall.S */
extern int mink_recv_ioctl(int, int, void *, uint32_t *);
extern int mink_recv_svc(void);
extern int mink_recv_skip(void);

static inline uintptr_t get_pc_from_ucontext(ucontext_t *ucontext) {
#if defined(__arm__) || defined(__aarch32__)  // ARM32
#if defined(__GLIBC__)
    return (uintptr_t)ucontext->uc_mcontext.arm_pc;
#else
    return (uintptr_t)ucontext->uc_mcontext.pc;
#endif
#else  // ARM64
    return (uintptr_t)ucontext->uc_mcontext.pc;
#endif
}

static inline void set_pc_in_ucontext(ucontext_t *ucontext, uintptr_t new_pc) {
#if defined(__arm__) || defined(__aarch32__)  // ARM32
#if defined(__GLIBC__)
    ucontext->uc_mcontext.arm_pc = new_pc;
#else
    ucontext->uc_mcontext.pc = new_pc;
#endif
#else  // ARM64
    ucontext->uc_mcontext.pc = new_pc;
#endif
}

/**
 * @brief Signal handler for terminating the supplicant thread
 *
 * This function handles the SIGUSR1 signal, which is registered
 * when the supplicant starts.
 *
 * It sets the TLS `sig_pending` variable to ensure the thread exits
 * when it checks this variable.
 * If the thread has already checked `sig_pending` but hasn't yet
 * entered the blocking `TEE_IOC_SUPPL_RECV` ioctl, it updates the
 * program counter (PC) to skip the ioctl. In this case, the `mink_recv_ioctl()`
 * function returns `-EINTR`, allowing the thread to terminate
 * synchronously and safely.
 */
static void supplicant_kill_handler(int sig, siginfo_t *info, void *context)
{
	(void)info;

	ucontext_t *ucontext = (ucontext_t *)context;
	uintptr_t pc_current = get_pc_from_ucontext(ucontext);
	uintptr_t addr_recv_ioctl = (uintptr_t)mink_recv_ioctl;
	uintptr_t addr_recv = (uintptr_t)mink_recv_svc;

	if (sig == SIGUSR1) {
		/* Are we just about to enter the syscall? Skip! */
		if (addr_recv_ioctl <= pc_current && pc_current <= addr_recv)
			set_pc_in_ucontext(ucontext, (uintptr_t)mink_recv_skip);
		else
			sig_pending = 1;
	}
}

/**
 * @brief Invoke a RECV or SEND call into TEE.
 *
 * This function implements a generic interface for invoking
 * `TEE_IOC_SUPPL_RECV` and `TEE_IOC_SUPPL_SEND` ioctls. Since
 * `TEE_IOC_SUPPL_RECV` is a blocking call, it is implemented separately via
 * the `mink_recv_ioctl()` function defined in syscall.S.
 *
 * The primary reason for this separation is to ensure the safe and synchronous
 * termination of supplicant threads by exiting the supplicant_worker()
 * function.
 *
 * The `mink_recv_ioctl` implementation checks the TLS `sig_pending` variable before
 * entering the kernel. If a kill signal is pending, the function skips
 * entering the kernel and terminates the thread by returning `-EINTR`.
 * If the signal arrives after the TLS `sig_pending` variable check, the
 * function skips entering the kernel by updating the program counter (PC) in
 * `supplicant_kill_handler()`.
 * If the signal arrives while being blocked on the `TEE_IOC_SUPPL_RECV` ioctl,
 * the kernel returns `-EINTR`, causing the thread to terminate synchronously
 * by exiting the `supplicant_worker()` function.
 * If the signal arrives at any other point, the function sets `sig_pending` to
 * 1 and waits for the thread to check it.
 *
 * @param fd File descriptor for the TEE device.
 * @param op Operation requested, must be either `TEE_IOC_SUPPL_RECV` or
*            `TEE_IOC_SUPPL_SEND`.
 * @param arg Argument for the ioctl.
 * @return Returns -EINTR if the system call was interrupted.
 *         Returns -EINVAL if invalid arguments were provided.
 */

#ifdef __GLIBC__
static int tee_call(int fd, unsigned long op, ...)
#else
static int tee_call(int fd, int op, ...)
#endif
{
	int ret = -1;
	sigset_t mask;

	va_list args;
	va_start(args, op);
	void *arg = va_arg(args, void *);
	va_end(args);

	switch (op) {
	case TEE_IOC_SUPPL_RECV:
		sigemptyset(&mask);
		sigaddset(&mask, SIGUSR1);

		/* It's safe to kill the thread here, UNBLOCK
		 * the kill signal */
		pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
		ret = mink_recv_ioctl(fd, op, arg, &sig_pending);
		pthread_sigmask(SIG_BLOCK, &mask, NULL);

		set_errno(ret);
		break;
	/* TEE_IOC_OBJECT_INVOKE
	 * TEE_IOC_SUPPL_SEND
	 * TEE_IOC_SHM_ALLOC
	 */
	default:
		ret = ioctl(fd, op, arg);
	}

	return ret;
}

/**
 * @brief Supplicant thread worker function.
 *
 * Worker function for the supplicant thread. This function calls
 * qcomtee_object_process_one() in a loop to receive, process and send a
 * response for the callback requests queued by QTEE.
 *
 * @param arg The root object with which this supplicant is associated.
 */
static void *supplicant_worker(void *arg)
{
	while (1) {
		if (qcomtee_object_process_one(arg))
			break;
	}

	return NULL;
}

/**
 * @brief Release the supplicant associated with a root object.
 *
 * This callback function is called by the QCOMTEE when the root object is
 * released, to allow the associated supplicant to release it's resources as
 * well.
 *
 * @param arg Supplicant which is to be released.
 */
static void supplicant_release(void *arg)
{
	struct supplicant *sup = (struct supplicant *)arg;
	int i;

	/* Here, we are sure there is no QTEE or callback object. In other
	 * words, there should not be anyone calling qcomtee_object_invoke
	 * or any request pending from QTEE. We issue the pthread_kill
	 * and wait for the thread to get cancelled.
	 */
	for (i = 0; i < sup->pthreads_num; i++)
		if (sup->pthreads[i].state)
			pthread_kill(sup->pthreads[i].thread, SIGUSR1);

	for (i = 0; i < sup->pthreads_num; i++)
		if (sup->pthreads[i].state)
			pthread_join(sup->pthreads[i].thread, NULL);

	free(sup);
}

struct supplicant *supplicant_start(int pthreads_num)
{
	int i, success = 0;
	struct supplicant *sup;

	struct sigaction action;

	/* Register the USR1 signal to enable graceful and
	 * synchronous termination of the supplicant threads
	 */
	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_SIGINFO;
	action.sa_sigaction = supplicant_kill_handler;
	sigaction(SIGUSR1, &action, NULL);

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	/* BLOCK the signal until it's safe to kill the
	 * thread
	 */
	sigprocmask(SIG_BLOCK, &mask, NULL);

	if (pthreads_num > SUPPLICANT_THREADS)
		return NULL;

	/* INIT all threads as SUPPLICANT_DEAD. */
	sup = calloc(1, sizeof(*sup));
	if (!sup)
		return NULL;

	/* Start a fresh namespace. */
	sup->root = qcomtee_object_root_init(DEV_TEE, tee_call, supplicant_release,
					     sup);
	if (sup->root == QCOMTEE_OBJECT_NULL)
		goto failed_out;

	sup->pthreads_num = pthreads_num;
	/* Start supplicant threads. */
	for (i = 0; i < sup->pthreads_num; i++) {
		if (!pthread_create(&sup->pthreads[i].thread, NULL,
				    supplicant_worker, sup->root)) {
			sup->pthreads[i].state = SUPPLICANT_RUNNING;
			success = 1;
		}
	}

	/* Success, if at least one thread has been started. */
	if (success)
		return sup;
failed_out:
	free(sup);

	return NULL;
}
