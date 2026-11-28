// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "memscpy.h"

#include "CListenerCBO.h"
#include "IListenerCBO_invoke.h"

#include "qlist.h"

#define MSGV printf
#define MSGD printf
#define MSGE printf

static inline void QList_free(QList *list)
{
	QNode *node = QList_pop(list);
	while (node) {
		free(node);
		node = QList_pop(list);
	}
}

/**
 * @brief Signal a waiting listener.
 *
 * Pop a waiter from the front of the queue and signal them about the
 * availability of the listener.
 */
static void signal_waiting_listener(ListenerCBO *me)
{
	wait_item *w_item = NULL;
	QNode *node = NULL;

	if (!me) {
		MSGE("[%s], cbo is null.", __FUNCTION__);
		return;
	}

	pthread_mutex_lock(&me->wait_mutex);

	if (!QList_isEmpty(&me->list_wait_cond)) {
		/* Client in the front of the queue, waiting for
		 * listener is freed first. On the basis of FIFO.
		 */
		node = QList_popLast(&me->list_wait_cond);
		w_item = container_of(node, wait_item, qn);
		pthread_cond_signal(&w_item->wait_cond);
	}

	pthread_mutex_unlock(&me->wait_mutex);
}

/**
 * @brief Retain the self Object.
 *
 * @param me The local object associated with this interface.
 * @return Object_OK
 */
static int32_t CListenerCBO_retain(ListenerCBO* me)
{
	atomic_fetch_add(&me->refs, 1);
	return Object_OK;
}

/**
 * @brief Release the self Object.
 *
 * @param me The local object associated with this interface.
 * @return Object_OK
 */
static int32_t CListenerCBO_release(ListenerCBO* me)
{
	if (atomic_fetch_sub(&me->refs, 1) == 1) {
		pthread_mutex_destroy(&me->wait_mutex);
		QList_free(&me->list_wait_cond);
		free(me);
		me = NULL;
	}

	return Object_OK;
}

/**
 * @brief Request a listener service on behalf of QTEE.
 *
 * QTEE requests a listener service by invoking a callback object associated
 * with the service.
 *
 * @param me The local object associated with this interface.
 * @return Object_OK on successful invocation of request.
 *         Object_ERROR otherwise.
 */
static int32_t CListenerCBO_request(ListenerCBO* me,
                                    uint32_t *emb_buf_offsets_ptr,
                                    size_t emb_buf_offsets_len,
                                    size_t *emb_buf_offsets_lenout,
                                    uint32_t *is_64_ptr,
                                    Object *smo1_ptr, Object *smo2_ptr,
                                    Object *smo3_ptr, Object *smo4_ptr)
{
	UNUSED(emb_buf_offsets_ptr);
	UNUSED(emb_buf_offsets_len);
	UNUSED(emb_buf_offsets_lenout);
	UNUSED(is_64_ptr);
	UNUSED(smo1_ptr);
	UNUSED(smo2_ptr);
	UNUSED(smo3_ptr);
	UNUSED(smo4_ptr);

	int ret = 0;
	int32_t rv = Object_OK;
	void *buf = me->buf;
	size_t buf_len = me->buf_len;
	char *tmp_buf = NULL;

	int expected = FREE; /* Expected state of listener */

	/* Check if the listener is FREE. Use strong ordering to eliminate the
	 * possibility of spurious failures.
	 */
	atomic_compare_exchange_strong(&me->listener_busy, &expected, BUSY);
	if (expected == BUSY)
		return Object_ERROR_BUSY;

	tmp_buf = (char *)calloc(1, buf_len);
	if (!tmp_buf) {
		rv = Object_ERROR_MEM;
		goto exit;
	}

	memscpy(tmp_buf, buf_len, buf, buf_len);

	/* It's possible to fail buf_len check inside dispatch
	 * so we must handle the error here by converting it to
	 * a transport Object_ERROR
	 */
	ret = me->dispatch_func(tmp_buf, buf_len);
	if (ret) {
		rv = Object_ERROR;
		MSGE("dispatch_func failed: %d\n", ret);
	}

	memscpy(buf, buf_len, tmp_buf, buf_len);
	free(tmp_buf);
exit:
	/* The listener is now FREE */
	atomic_exchange(&me->listener_busy, FREE);

	/* Signal the TAs in QTEE waiting on this listener for availability */
	signal_waiting_listener(me);

	return rv;
}

/**
 * @brief Wait for a listener service on behalf of QTEE.
 *
 * QTEE service or TA which is waiting for a listener is pushed into a queue.
 * The client pushed into the queue is present there for a maximum for 4.5
 * seconds, since the Mink Adaptor timout is 5 seconds, otherwise it results
 * in a timeout error.
 *
 * @param me The local object associated with this interface.
 * @return Object_OK on successful invocation of request.
 *         Object_ERROR otherwise.
 */
static int32_t CListenerCBO_wait(ListenerCBO *me)
{
	int32_t cond_ret_val = 0;
	int32_t rv = Object_OK;
	struct timespec wakeup_time;

	/* Don't wait if the listener is already available */
	if (atomic_load(&me->listener_busy) == FREE)
		return Object_OK;

	/* Now wait... */
	pthread_mutex_lock(&me->wait_mutex);

	/* Create a condition variable for each client that calls into
	 * wait in the QTEE for specific listener.
	 */
	wait_item *w_item = (wait_item *)calloc(1, sizeof(wait_item));
	if (!w_item)
		return Object_ERROR_KMEM;

	QNode_construct(&w_item->qn);
	pthread_cond_init(&w_item->wait_cond, NULL);

	/* Client waiting for the listener is pushed into a list */
	QList_appendNode(&me->list_wait_cond, (QNode *)w_item);

	/* Current client, which called into wait, will wait for the
         * listener to be be freed till signalled.
         * The client in the queue will wait for a maximum of given duration
	 */
	clock_gettime(CLOCK_REALTIME, &wakeup_time);

	/* Mink Adaptor timeout is 5 seconds.
         * So within 4.5 seconds the request that is waiting to be signalled
	 * should be addessed, else it will result in timeout error.
	 */
	wakeup_time.tv_sec += 4.5;

	cond_ret_val = pthread_cond_timedwait(&w_item->wait_cond,
					      &me->wait_mutex, &wakeup_time);
	if(cond_ret_val != 0) {
		if(cond_ret_val == ETIMEDOUT) {
			MSGE("[%s], PID : %d, Timed out: The max limit on wait"
			      " timedout : %s (%d) for lid : %d\n", __FUNCTION__,
			      getpid(), strerror(errno), errno, me->listener_id);

			/* In case of a timeout, dont send error back to TZ.
			 * Instead, we want the request to be lined up again.
			 */
			rv =  Object_OK;
		} else {
			MSGE("[%s], PID : %d, pthread_cond_timedwait failed :"
			      "%s, (%d) for lid : %d\n", __FUNCTION__, getpid(),
			      strerror(errno), errno, me->listener_id);

			rv = Object_ERROR;
		}

		QList_popLast(&me->list_wait_cond);
	}

	/* In-case of success, signalling thread removes wait_item from list */

	pthread_cond_destroy(&w_item->wait_cond);
	free(w_item);

	pthread_mutex_unlock(&me->wait_mutex);

	return rv;
}

static IListenerCBO_DEFINE_INVOKE(CListenerCBO_invoke, CListenerCBO_,
				  ListenerCBO*)

int32_t CListenerCBO_new(Object *obj_out, int listener_id, dispatch_entry dispatch_func,
			 void *buf, size_t buf_len)
{
	ListenerCBO *me = NULL;

	me = (ListenerCBO *)malloc(sizeof(ListenerCBO));
	if (!me)
		return Object_ERROR_KMEM;

	atomic_init(&me->refs, 1);
	me->buf = buf;
	me->buf_len = buf_len;
	me->dispatch_func = dispatch_func;
	me->listener_id = listener_id;

	QList_construct(&me->list_wait_cond);
	atomic_init(&me->listener_busy, FREE);
	pthread_mutex_init(&me->wait_mutex, NULL);

	*obj_out = (Object){ CListenerCBO_invoke, me };
	return Object_OK;
}
