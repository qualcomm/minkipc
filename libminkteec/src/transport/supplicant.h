// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _SUPPLICANT_H
#define _SUPPLICANT_H

/* Callback supplicant for the direct libqcomtee backend.
 *
 * Copy of libminkadaptor/src/supplicant.h. libminkteec needs its own because
 * with MINKTEEC_DIRECT_QCOMTEE it no longer links libminkadaptor, yet it still
 * has to service the callback requests QTEE raises against the waiter object.
 * Keep the two in sync; they are byte identical apart from this comment.
 */

#include <qcomtee_object_types.h>

#define set_errno(e) (errno = (-e))

/* Driver's file.*/
#define DEV_TEE "/dev/tee0"

/* Maximum number of threads which can be associated with the Supplicant */
#define SUPPLICANT_THREADS 4

/* The Supplicant is either dead or running! */
#define SUPPLICANT_DEAD 0
#define SUPPLICANT_RUNNING 1
struct supplicant {
	int pthreads_num;

	struct {
		int state;
		pthread_t thread;
	} pthreads[SUPPLICANT_THREADS];

	struct qcomtee_object *root;
};

/**
 * @brief Start a new supplicant associated with a root object.
 *
 * Called by the client when it needs a callback supplicant for servicing
 * callback requests received from QTEE in-response to the callback objects
 * sent by it.
 *
 * @param pthread_num Number of threads to create and associate with the
 *                    supplicant.
 * @return Returns a supplicant on success.
 *         Returns NULL on failure.
 */
struct supplicant *supplicant_start(int pthreads_num);

#endif // _SUPPLICANT_H
