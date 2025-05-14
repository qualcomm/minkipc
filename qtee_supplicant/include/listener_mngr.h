// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __LISTENER_MNGR_H
#define __LISTENER_MNGR_H

#include <stdint.h>
#include <stdio.h>
#include "object.h"

#define MSGV printf
#define MSGD printf
#define MSGE printf

typedef int (*svc_init)(void);
typedef int32_t (*svc_register)(Object root);
typedef void (*svc_deregister)(void);

/**
 * @brief Listener service.
 *
 * Represents a listener service to be initialized and started by the QTEE
 * supplicant. Each listener service offers a specific REE service to QTEE,
 * e.g. time service.
 */
struct listener_svc {
	char *service_name; /**< Name of the listener service. */
	int id; /**< Id of the listener service. */
	int is_registered; /**< Listener registration status. */
	char *file_name; /**< File name of the listener service. */
	void *lib_handle; /**< LibHandle for the listener. */
	char *svc_init; /**< Listener service (opt) init callback. */
	char *svc_register; /**< Listener service (opt) register callback. */
	char *svc_deregister; /**< Listener service (opt) deregister callback. */
	char *smci_dispatch; /**< Dispatch function of the listener. */
	Object cbo; /**< Store cbo of listener service. */
	Object mo; /**< Store memory object for this listener shared with QTEE. */
	size_t buf_len; /**< Buffer length for listener service. */
};

/**
 * @brief Initialize listener services.
 *
 * Initializes a listener service by invoking the init callback defined by the
 * listener.
 */
int init_listener_services(void);

/**
 * @brief Start listener services.
 *
 * Starts listener services which wait for a listener request from QTEE.
 */
int start_listener_services(void);

#endif // __LISTENER_MNGR_H
