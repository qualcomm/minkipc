// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "gp_wait.h"

#define TYPE_WAITER_ITEM 0
#define TYPE_SIGNAL_ITEM 1

#define MSEC_PER_SEC (1000L)

typedef struct {
	uint32_t events;
	uint32_t code;
	bool signaled;
	/* Protect the CV */
	pthread_mutex_t mutex;
	pthread_cond_t condition;
} waiter_item;

typedef struct {
	uint32_t events;
	uint32_t code;
} signal_item;

typedef struct {
	QNode qn;

	uint8_t type;
	union {
		waiter_item w_item;
		signal_item s_item;
	} item;
} list_item;

/* Scalar layout of the IWait methods on the wire. The three in-parameters of
 * IWait.wait share one input buffer and its single out-parameter gets an output
 * buffer of its own; IWait.signal has no out-parameter at all.
 */
struct wait_in {
	uint32_t msec;
	uint32_t code;
	uint32_t events;
};
_Static_assert(sizeof(struct wait_in) == 12, "IWait.wait in-scalar pack drift");

struct signal_in {
	uint32_t code;
	uint32_t events;
};
_Static_assert(sizeof(struct signal_in) == 8,
	       "IWait.signal in-scalar pack drift");

/* Output buffer handed back to QTEE for IWait.wait.
 *
 * The transport reads it while submitting the response, which happens after
 * the dispatcher has returned but still inside the same
 * qcomtee_object_process_one() call on this very thread. Thread-local storage
 * covers exactly that lifetime: concurrent waits on the same object cannot
 * clobber each other, and there is nothing left to free once the response is
 * out. libminkadaptor instead malloc()s a buffer per dispatch and frees it from
 * its error hook, which does not survive two dispatches being in flight on one
 * object.
 */
static __thread uint32_t events_out_buf;

/**
 * @brief Signal the waiter with the specified event(s).
 *
 * @param list A pointer to the list of waiter/signal items.
 * @param code The unique identifier code for the event to signal.
 * @param events The type of event to signal.
 * @return bool TRUE if the waiter was signalled.
 *              FALSE if the waiter wasn't signalled.
 */
static bool signal_waiter_item(QList *list, uint32_t code, uint32_t events)
{
	QNode *node = NULL;
	list_item *l_item = NULL;
	waiter_item *w_item = NULL;
	bool signaled = false;

	QLIST_FOR_ALL(list, node)
	{
		l_item = container_of(node, list_item, qn);
		if (l_item->type != TYPE_WAITER_ITEM)
			continue;

		w_item = &l_item->item.w_item;
		if ((w_item->events & events) &&
		    ((w_item->code == 0 || w_item->code == code))) {
			pthread_mutex_lock(&w_item->mutex);

			w_item->signaled = true;
			w_item->events &= events;

			/* Match, wake up the waiter! */
			pthread_cond_signal(&w_item->condition);
			signaled = true;

			pthread_mutex_unlock(&w_item->mutex);

			/* Multiple (event, code) pairs not possible */
			break;
		}
	}

	return signaled;
}

/**
 * @brief Find a signal item in the waiter/signal list.
 *
 * @param list A pointer to the list of waiter/signal items.
 * @param code The unique identifier code for the event to signal.
 * @param events The type of event to signal.
 * @param events_out The type of event returned to QTEE.
 * @return bool TRUE if the signal item was found.
 *              FALSE if the signal item was not found.
 */
static bool get_signal_item(QList *list, uint32_t code, uint32_t events,
			    uint32_t *events_out)
{
	QNode *node = NULL;
	list_item *l_item = NULL;
	signal_item *s_item = NULL;
	bool found = false;

	QLIST_FOR_ALL(list, node)
	{
		l_item = container_of(node, list_item, qn);
		if (l_item->type != TYPE_SIGNAL_ITEM)
			continue;

		s_item = &l_item->item.s_item;
		if ((s_item->events & events) &&
		    ((s_item->code == code || s_item->code == 0))) {
			*events_out = (s_item->events & events);

			found = true;
			break;
		}
	}

	if (found) {
		QNode_dequeue(node);
		free(l_item);
		l_item = NULL;
	}

	return found;
}

/**
 * @brief Adds a signal item to the waiter/signal list.
 *
 * @param list A pointer to the list of waiter/signal items.
 * @param code The unique identifier code for the event to signal.
 * @param events The type of event to signal.
 * @return list_item The newly queued signal item.
 *         NULL on failure.
 */
static list_item *queue_signal_item(QList *list, uint32_t code, uint32_t events)
{
	list_item *l_item = (list_item *)malloc(sizeof(list_item));
	if (!l_item)
		return NULL;

	memset(l_item, 0, sizeof(list_item));
	QNode_construct(&l_item->qn);
	l_item->type = TYPE_SIGNAL_ITEM;
	l_item->item.s_item.events = events;
	l_item->item.s_item.code = code;

	QList_appendNode(list, (QNode *)l_item);

	return l_item;
}

/**
 * @brief Adds a waiter item to the waiter/signal list.
 *
 * @param list A pointer to the list of waiter/signal items.
 * @param code The unique identifier code for the event to wait for.
 * @param events The type of event to wait for.
 * @return list_item The newly queued waiter item.
 *         NULL on failure.
 */
static list_item *queue_waiter_item(QList *list, uint32_t code, uint32_t events)
{
	list_item *l_item = (list_item *)malloc(sizeof(list_item));
	if (!l_item)
		return NULL;

	memset(l_item, 0, sizeof(list_item));
	QNode_construct(&l_item->qn);
	l_item->type = TYPE_WAITER_ITEM;

	waiter_item *w_item = &(l_item->item.w_item);
	w_item->events = events;
	w_item->code = code;
	w_item->signaled = false;
	pthread_mutex_init(&w_item->mutex, NULL);
	pthread_cond_init(&w_item->condition, NULL);

	QList_appendNode(list, (QNode *)l_item);

	return l_item;
}

/**
 * @brief Dequeue and clear the waiter item from the list.
 *
 * @param l_item The waiter item to be cleared.
 */
static void clear_waiter_item(list_item *l_item)
{
	QNode_dequeue(&l_item->qn);

	waiter_item *w_item = &(l_item->item.w_item);
	pthread_mutex_destroy(&w_item->mutex);
	pthread_cond_destroy(&w_item->condition);

	free(l_item);
	l_item = NULL;
}

/**
 * @brief Free every item left on the waiter/signal list.
 *
 * @param list A pointer to the list of waiter/signal items.
 */
static void QList_free(QList *list)
{
	QNode *node = QList_pop(list);
	while (node) {
		free(node);
		node = QList_pop(list);
	}
}

/**
 * @brief Compute the wakeup time in terms of timespec from milliseconds.
 *
 * @param msec Time in milliseconds before we wake up.
 * @param wakeup wakeup time in terms of timespec.
 */
static void compute_wakeup_time(uint32_t msec, struct timespec *wakeup)
{
	struct timespec start;
	clock_gettime(CLOCK_REALTIME, &start);

	/* pthread_cond_timedwait() resolves to the system clock, which
	 * has OS tick resolution. We account for this by waiting at
	 * least that long to guarantee TEE_Wait() times.
	 */
	long tick_sec = sysconf(_SC_CLK_TCK);
	if ((tick_sec > 0) && (msec < (uint32_t)(MSEC_PER_SEC / tick_sec)))
		msec = (uint32_t)(MSEC_PER_SEC / tick_sec);

	/* We don't consider the case of tv_sec overflowing (it's UTC
	 * and that's a time and date quite a bit in the future), and
	 * tv_nsec cannot overflow, since it comes as the sum of number
	 * from a system API (start.tv_nsec), therefore < 1e9, and a
	 * number which is at most 1e9-1 (999 ms)
	 */
	wakeup->tv_sec = start.tv_sec + (time_t)(msec / 1000);
	wakeup->tv_nsec = start.tv_nsec + (long)((msec % 1000) * 1000000);
	if (wakeup->tv_nsec > 1000000000) {
		wakeup->tv_sec += (wakeup->tv_nsec / 1000000000);
		wakeup->tv_nsec %= 1000000000;
	}
}

/**
 * @brief Wait for the specified amount of milliseconds for an event to occur.
 *
 * @param w_item The waiter item representing this wait request.
 * @param wakeup Time until wakeup from the waiting request.
 * @param msec Time in milliseconds for which to wait.
 * @param events_out The event signalled to the waiter item.
 * @return QCOMTEE_OK on success.
 *         QCOMTEE_ERROR on failure.
 */
static qcomtee_result_t wait_for_signal(waiter_item *w_item,
					struct timespec wakeup, uint32_t msec,
					uint32_t *events_out)
{
	qcomtee_result_t rv = QCOMTEE_OK;
	int wait_ret = 0;

	pthread_mutex_lock(&w_item->mutex);
	while (!w_item->signaled) {
		if (msec != MINK_WAIT_INFINITE) {
			/* We use pthread_cond_timedwait() to sleep.
			* If the specified time is in the past, then we will
			* get an ETIMEDOUT return. If it returns with 0, and
			* the condition wasn't signaled, then it was a spurious
			* wake up, and we simply sleep again.
			*/
			wait_ret = pthread_cond_timedwait(
				&w_item->condition, &w_item->mutex, &wakeup);
			if (wait_ret == ETIMEDOUT)
				break;
		} else {
			wait_ret = pthread_cond_wait(&w_item->condition,
						     &w_item->mutex);
		}

		if (wait_ret) {
			rv = QCOMTEE_ERROR;
			break;
		}

		/* If wait_ret is 0, it's still possible that we suffered a
		 * spurious wake-up. To ensure that we were correctly woken up,
		 * we check the w_item->signaled variable at the top of the
		 * loop. If this was in-fact a spurious wakeup call,
		 * we would sleep less this time around.
		 */
	}
	pthread_mutex_unlock(&w_item->mutex);

	if (w_item->signaled)
		/* Report the event that was signaled */
		*events_out = w_item->events;

	return rv;
}

/**
 * @brief Wait for the specified amount of milliseconds for an event to occur.
 *
 * @param me The waiter to block on.
 * @param msec How long to wait, in milliseconds, or MINK_WAIT_INFINITE.
 * @param code Optional code to match an incoming signal against.
 * @param events The mask of events to wait for.
 * @param events_out The mask of events that were actually signalled.
 * @return QCOMTEE_OK on success.
 *         QCOMTEE_ERROR* on failure.
 */
static qcomtee_result_t cwait_wait(struct cwait *me, uint32_t msec,
				   uint32_t code, uint32_t events,
				   uint32_t *events_out)
{
	qcomtee_result_t rv = QCOMTEE_OK;
	list_item *l_item = NULL;
	waiter_item *w_item = NULL;
	/* Only read when msec is finite, but wait_for_signal() takes it by
	 * value, so leave nothing indeterminate.
	 */
	struct timespec wakeup = { 0 };

	if (events == MINK_EVENT_NONE || msec == 0) {
		*events_out = 0;
		return QCOMTEE_OK;
	}

	pthread_mutex_lock(&me->lock);
	/* We are being asked to wait with a non-zero millisecond timeout.
	 * But first, let's check if there is a signal pending for us, if so
	 * we'll return early!
	 */
	if (get_signal_item(&me->list, code, events, events_out)) {
		pthread_mutex_unlock(&me->lock);
		return QCOMTEE_OK;
	}

	/* Nobody queued a signal for this wait request, so now we have to
	 * queue a waiter item (to receive the signal) and wait.
	 */
	l_item = queue_waiter_item(&me->list, code, events);
	if (!l_item) {
		pthread_mutex_unlock(&me->lock);
		return QCOMTEE_ERROR_MEM;
	}

	w_item = &(l_item->item.w_item);
	pthread_mutex_unlock(&me->lock);

	/* Compute our wakeup/wait time in preparation of signal wait */
	if (msec != MINK_WAIT_INFINITE)
		compute_wakeup_time(msec, &wakeup);

	rv = wait_for_signal(w_item, wakeup, msec, events_out);

	/* Clear the waiter item */
	pthread_mutex_lock(&me->lock);
	clear_waiter_item(l_item);
	pthread_mutex_unlock(&me->lock);

	return rv;
}

qcomtee_result_t cwait_signal(struct cwait *me, uint32_t code, uint32_t events)
{
	bool signaled = false;

	pthread_mutex_lock(&me->lock);

	signaled = signal_waiter_item(&me->list, code, events);

	if (!signaled && code)
		/* If nobody was waiting on this signal, we queue it to the
		 * list, since the TA can attempt to wait on it later.
		 * Note that we do this only if a cancel code is passed, i.e.
		 * signals with no cancel code that don't find a matching waiter
		 * are ignored.
		 */
		queue_signal_item(&me->list, code, events);

	pthread_mutex_unlock(&me->lock);

	return QCOMTEE_OK;
}

/**
 * @brief Dispatch an IWait invocation coming from QTEE.
 *
 * Takes the place of the idlc-generated IWait_invoke() skeleton: the scalars
 * are read straight out of the input buffer instead of being funnelled through
 * an ObjectArg array.
 *
 * @param object The waiter being invoked.
 * @param op Operation requested by QTEE.
 * @param params Parameter array for the invocation.
 * @param num Number of parameters in the params array.
 * @return QCOMTEE_OK on success.
 *         QCOMTEE_ERROR* on failure.
 */
static qcomtee_result_t cwait_dispatch(struct qcomtee_object *object,
				       qcomtee_op_t op,
				       struct qcomtee_param *params, int num)
{
	struct cwait *me = CWAIT_OF(object);
	struct wait_in w_in;
	struct signal_in s_in;

	/* QTEE's operation reaches the dispatcher verbatim, so take the method
	 * bits the way the generated skeleton's ObjectOp_methodID() would. The
	 * mask is idempotent on a bare method ID. QCOMTEE_OBJREF_OP_RELEASE is
	 * handled by libqcomtee before we ever get here.
	 */
	switch (op & MINK_OP_METHOD_MASK) {
	case MINK_OP_WAIT_WAIT:
		if (num != 2 || params[0].attr != QCOMTEE_UBUF_INPUT ||
		    params[1].attr != QCOMTEE_UBUF_OUTPUT ||
		    params[0].ubuf.size < sizeof(w_in) ||
		    params[1].ubuf.size < sizeof(events_out_buf))
			return QCOMTEE_ERROR_INVALID;

		/* The input buffer belongs to the transport and carries no
		 * alignment guarantee of its own, hence the copy.
		 */
		memcpy(&w_in, params[0].ubuf.addr, sizeof(w_in));

		events_out_buf = 0;
		params[1].ubuf.addr = &events_out_buf;
		params[1].ubuf.size = sizeof(events_out_buf);

		return cwait_wait(me, w_in.msec, w_in.code, w_in.events,
				  &events_out_buf);

	case MINK_OP_WAIT_SIGNAL:
		if (num != 1 || params[0].attr != QCOMTEE_UBUF_INPUT ||
		    params[0].ubuf.size < sizeof(s_in))
			return QCOMTEE_ERROR_INVALID;

		memcpy(&s_in, params[0].ubuf.addr, sizeof(s_in));

		return cwait_signal(me, s_in.code, s_in.events);

	default:
		return QCOMTEE_ERROR_INVALID;
	}
}

/**
 * @brief Notify the waiter of the transport state after a dispatch.
 *
 * @param object The waiter that was invoked.
 * @param err State of transport (0 is success; Otherwise error).
 */
static void cwait_error(struct qcomtee_object *object, int err)
{
	(void)object;
	(void)err;

	/* Nothing to unwind. The only per-dispatch state is events_out_buf,
	 * which is thread-local and needs no cleanup. The hook stays wired so
	 * that a failed response remains a defined event for this object.
	 */
}

/**
 * @brief Release the waiter.
 *
 * Called by libqcomtee once the last reference is gone, which implies nobody is
 * blocked in cwait_wait() any more; whatever is left on the list are signals
 * that never found a waiter.
 *
 * @param object The waiter being released.
 */
static void cwait_release(struct qcomtee_object *object)
{
	struct cwait *me = CWAIT_OF(object);

	QList_free(&me->list);
	pthread_mutex_destroy(&me->lock);
	free(me);
}

/* .supported stays NULL: only objects QTEE probes for capabilities, such as
 * IClientEnv, need it. .dispatch must not be NULL or qcomtee_object_cb_init()
 * refuses to initialise the object.
 */
static struct qcomtee_object_ops cwait_ops = {
	.release = cwait_release,
	.dispatch = cwait_dispatch,
	.error = cwait_error,
	.supported = NULL,
};

int32_t cwait_open(struct qcomtee_object *root, struct qcomtee_object **objOut)
{
	struct cwait *me = (struct cwait *)malloc(sizeof(struct cwait));
	if (!me)
		return QCOMTEE_ERROR_MEM;

	memset(me, 0, sizeof(struct cwait));

	QList_construct(&me->list);
	pthread_mutex_init(&me->lock, NULL);

	/* Takes a reference on root and starts the object off with a single
	 * reference of its own. It only fails when ops->dispatch is NULL, but
	 * check regardless so that editing cwait_ops cannot silently leak.
	 */
	if (qcomtee_object_cb_init(&me->object, &cwait_ops, root)) {
		pthread_mutex_destroy(&me->lock);
		free(me);

		return QCOMTEE_ERROR;
	}

	*objOut = &me->object;

	return QCOMTEE_OK;
}
