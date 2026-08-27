// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __GP_WAIT_H_
#define __GP_WAIT_H_

/* Cancellation waiter of the direct libqcomtee backend.
 *
 * This is CWait.c re-based on libqcomtee: instead of handing QTEE a MINK
 * Object that libminkadaptor has to wrap in a callback object, the waiter is
 * itself a struct qcomtee_object and implements qcomtee_object_ops directly.
 * QTEE's IWait.wait therefore lands in cwait_dispatch() straight from the
 * supplicant thread, and TEEC_RequestCancellation() signals the very same
 * object locally, without any IPC.
 *
 * Only built when MINKTEEC_DIRECT_QCOMTEE is on.
 */

#include <pthread.h>
#include <stdint.h>

/* qcomtee_object.h has to come before qlist.h: both define container_of and
 * only qlist.h guards its definition.
 */
#include "qcomtee_errno.h"
#include "qcomtee_object.h"

#include "qlist.h"

/* IWait constants, restated from idl/IWait.idl:17-27 so that the direct
 * backend carries no dependency on the idlc output.
 */
#define MINK_WAIT_INFINITE 0xFFFFFFFF
#define MINK_EVENT_NONE 0
#define MINK_EVENT_CANCEL 1

/* IWait method IDs, in the declaration order of idl/IWait.idl:45-56. */
#define MINK_OP_WAIT_WAIT 0
#define MINK_OP_WAIT_SIGNAL 1

/* Method bits of an operation, as ObjectOp_methodID() masks them
 * (object.h:30/:37). QTEE's operation reaches the dispatcher verbatim, so the
 * waiter masks it the same way the idlc skeleton would.
 */
#define MINK_OP_METHOD_MASK 0x0000FFFFu

/**
 * @brief The cancellation waiter.
 *
 * Holds a single list of both pending signals and blocked waiters; @ref lock
 * protects that list, while each waiter item carries its own mutex and
 * condition variable. Reference counting lives in @ref object and is driven by
 * libqcomtee, hence there is no refcount of our own.
 */
struct cwait {
	struct qcomtee_object object;

	QList list;
	/* Protect the QList */
	pthread_mutex_t lock;
};

#define CWAIT_OF(o) container_of((o), struct cwait, object)

/**
 * @brief Create a cancellation waiter.
 *
 * @param root The root object the waiter belongs to. A callback object can
 *             only be exchanged with QTEE objects sharing its root, so this
 *             has to be the same root the session is opened against.
 * @param objOut The waiter, owned by the caller with a single reference.
 * @return QCOMTEE_OK on success.
 *         QCOMTEE_ERROR* on failure.
 */
int32_t cwait_open(struct qcomtee_object *root, struct qcomtee_object **objOut);

/**
 * @brief Signal the waiter with the specified event(s).
 *
 * Called locally by TEEC_RequestCancellation(); QTEE only ever waits on the
 * object, it does not signal it.
 *
 * @param me The waiter to signal.
 * @param code The unique identifier code for the event to signal.
 * @param events The type of event to signal.
 * @return QCOMTEE_OK.
 */
qcomtee_result_t cwait_signal(struct cwait *me, uint32_t code, uint32_t events);

#endif // __GP_WAIT_H_
