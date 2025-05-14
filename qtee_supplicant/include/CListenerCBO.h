// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __CLISTENERCBO_H
#define __CLISTENERCBO_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#include "listener_mngr.h"
#include "object.h"
#include "qlist.h"

#define UNUSED(x) (void)(x)
#define BUSY 1
#define FREE 0

/* Pointer to listener dispatch function */
typedef int (*dispatch_entry)(void*, size_t);

/**
 * @brief Waiter item
 *
 * A Qlist waiter item representing a conditional variable.
 */
typedef struct {
	QNode qn;
	pthread_cond_t wait_cond;
} wait_item;

/**
 * @brief Listener Callback Object
 *
 * A listener callback object invoked by QTEE to request a particular listener
 * service.
 */
typedef struct {
	atomic_int refs; /**< Reference count of the object. */
	int listener_id; /**< The id of the listener represented by this object. */
	Object smo; /**< The memory object shared between listener and QTEE.  */
	dispatch_entry smci_dispatch_func; /**< Dispatch function for the listener. */
	pthread_mutex_t wait_mutex; /**< QTEE waits on this mutex for the listener. */
	QList list_wait_cond; /**< Queue of QTEE services/TAs waiting on listener. */
	atomic_int listener_busy; /**< Busy status of the listener. */
} ListenerCBO;

/**
 * @brief Create a new listener callback object
 *
 * Creates a new listener callback object which can be registered with QTEE by
 * the QTEE supplicant.
 */
int32_t CListenerCBO_new(Object *objOut, Object smo, struct listener_svc *listener);

#endif // __CLISTENERCBO_H
