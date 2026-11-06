// Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _FSP_INTERNAL_H_
#define _FSP_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>

#include "object.h"

#define C_LENGTHOF(array) \
	((void *)&(array) == (void *)(array) ? sizeof(array) / sizeof *(array) : 0)

#define LOG_ERR(fmt, ...)                                                      \
	fprintf(stderr, "FSP_CLIENT ERROR [%s:%d]: " fmt, __func__, __LINE__, \
		##__VA_ARGS__)

#define CHECK_COND_ERR(cond, err_code)   \
	if (!(cond)) {                   \
		ret = err_code;           \
		LOG_ERR("condition failed: %s\n", #cond); \
		goto exit;                \
	}

#define DGUARD(func, expected_value, err_code) \
	CHECK_COND_ERR((expected_value) == (func), err_code);

/* Command IDs accepted on the CLI (-c / --cmd_id). */
typedef enum {
	FSP_GET_PUBLIC_KEY = 1,
	FSP_PROVISION_KEY = 2,
	FSP_VERIFY_KEY = 3,
	FSP_GET_PUBLIC_KEY_V2 = 4,
	FSP_PROVISION_KEY_V2 = 5,
} fsp_cmd_t;

/* Handles for the loaded fspapp TA. */
typedef struct {
	Object app_controller; /* IAppController returned by IAppLoader_loadFromBuffer() */
	Object fsp_ta;         /* IFSPApp object obtained via IAppController_getAppObject() */
} fsp_usecase_handler;

/* Buffer shared with QTEE via a MinkCom Memory object. */
typedef struct {
	uint8_t *cpu_va;
	size_t size;
	Object memory_obj;
} fsp_buf_t;

#endif /* _FSP_INTERNAL_H_ */
