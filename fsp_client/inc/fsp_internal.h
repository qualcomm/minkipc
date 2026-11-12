// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _FSP_INTERNAL_H_
#define _FSP_INTERNAL_H_

#include <stddef.h>
#include <stdio.h>

#include "object.h"

#define LOG_ERR(fmt, ...)                                                      \
	fprintf(stderr, "FSP_CLIENT ERROR [%s:%d]: " fmt, __func__, __LINE__, \
		##__VA_ARGS__)

/* Command IDs accepted on the CLI (-c / --cmd_id). */
typedef enum {
	FSP_GET_PUBLIC_KEY = 1,
	FSP_PROVISION_KEY = 2,
} fsp_cmd_t;

/* Handles for the loaded fspapp TA. */
typedef struct {
	Object app_controller; /* IAppController returned by IAppLoader_loadFromBuffer() */
	Object fsp_ta;         /* IFSPApp object obtained via IAppController_getAppObject() */
} fsp_usecase_handler;

/* Buffer shared with QTEE via a MinkCom memory object. */
typedef struct {
	void *cpu_va;
	size_t size;
	Object memory_obj;
} fsp_buf_t;

#endif /* _FSP_INTERNAL_H_ */
