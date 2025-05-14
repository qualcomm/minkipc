// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "heap.h"
#include "listener_mngr.h"
#include "memscpy.h"
#include "MinkCom.h"

#include "CListenerCBO.h"
#include "IListenerCBO_invoke.h"

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

	if (!me) {
		MSGE("[%s], cbo is null.", __FUNCTION__);
		return;
	}

	pthread_mutex_lock(&me->wait_mutex);

	if (!QList_isEmpty(&me->list_wait_cond)) {
		/* Client in the front of the queue, waiting for
		 * listener is freed first. On the basis of FIFO.
		 */
		w_item = (wait_item *)QList_popLast(&me->list_wait_cond);
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
		Object_ASSIGN_NULL(me->smo);
		pthread_mutex_destroy(&me->wait_mutex);
		QList_free(&me->list_wait_cond);
		HEAP_FREE_PTR(me);
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
	void *dma_buf = NULL;
	size_t dma_buf_len = 0;
	char *tmp_buf = NULL;

	int expected = FREE; /* Expected state of listener */

	/* Check if the listener is FREE. Use strong ordering to eliminate the
	 * possibility of spurious failures.
	 */
	atomic_compare_exchange_strong(&me->listener_busy, &expected, BUSY);
	if (expected == BUSY)
		return Object_ERROR_BUSY;

	rv = MinkCom_getMemoryObjectInfo(me->smo, &dma_buf, &dma_buf_len);
	if (Object_isERROR(rv)) {
		MSGE("getMemoryObjectInfo failed: 0x%x\n", rv);
		goto exit;
	}

	tmp_buf = (char *)calloc(1, dma_buf_len);
	if (!tmp_buf) {
		rv = Object_ERROR_MEM;
		goto exit;
	}

	memscpy(tmp_buf, dma_buf_len, dma_buf, dma_buf_len);

	/* It's possible to fail dma_buf_len check inside dispatch
	 * so we must handle the error here by converting it to
	 * a transport Object_ERROR
	 */
	ret = me->smci_dispatch_func(tmp_buf, dma_buf_len);
	if (ret) {
		rv = Object_ERROR;
		MSGE("smci_dispatch_func failed: %d\n", ret);
	}

	memscpy(dma_buf, dma_buf_len, tmp_buf, dma_buf_len);

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
	wait_item *w_item = (wait_item *)malloc(sizeof(wait_item));
	if (!w_item)
		return Object_ERROR_KMEM;

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

int32_t CListenerCBO_new(Object *obj_out, Object smo,
			 struct listener_svc *listener)
{
	int ret = 0;
	dispatch_entry disp_entry;
	ListenerCBO* me = NULL;

	me = HEAP_ZALLOC_TYPE(ListenerCBO);
	if (!me)
		return Object_ERROR_KMEM;

	disp_entry = (dispatch_entry)dlsym(listener->lib_handle,
					   listener->smci_dispatch);
	if (disp_entry == NULL) {
		MSGE("dlsym(%s) not found in lib %s, dlerror msg: %s\n",
		      listener->smci_dispatch, listener->file_name, dlerror());
		ret = Object_ERROR;
		goto err;
	}

	atomic_init(&me->refs, 1);
	me->smci_dispatch_func = disp_entry;
	me->listener_id = listener->id;
	Object_INIT(me->smo, smo);

	QList_construct(&me->list_wait_cond);
	atomic_init(&me->listener_busy, FREE);
	pthread_mutex_init(&me->wait_mutex, NULL);

	*obj_out = (Object){ CListenerCBO_invoke, me };
	return Object_OK;

err:
	MSGE("Ctor of CListenerCBO failed and is bailing due to error\n");
	HEAP_FREE_PTR(me);

	return ret;
}
