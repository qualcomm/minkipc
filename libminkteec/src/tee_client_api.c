// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/* The nine entry points this file calls down into are declared by the active
 * backend's private header rather than by the shared mink_params.h, so that each
 * backend owns the contract it implements. The generated config header has to
 * come first: the switch is needed here before anything that would pull it in
 * through tee_client_api.h has been included.
 */
#include "minkteec_config.h"

#if MINKTEEC_DIRECT_QCOMTEE
#include "gp_teec.h"
#else
#include "mink_teec.h"
#endif

static int verify_shm(TEEC_RegisteredMemoryReference memref,
		      TEEC_SharedMemory *shm, TEEC_Context *ctx, uint32_t type)
{
	if (shm->imp.ctx != ctx)
		return -1;

	if (type == TEEC_MEMREF_PARTIAL_INPUT ||
	    type == TEEC_MEMREF_PARTIAL_INOUT)
		if (!(shm->flags & TEEC_MEM_INPUT))
			return -1;

	if (type == TEEC_MEMREF_PARTIAL_OUTPUT ||
	    type == TEEC_MEMREF_PARTIAL_INOUT)
		if (!(shm->flags & TEEC_MEM_OUTPUT))
			return -1;

	if (type != TEEC_MEMREF_WHOLE)
		if (memref.offset + memref.size > shm->size)
			return -1;

	return 0;
}

static TEEC_Result verify_params(TEEC_Context *ctx, uint32_t param_types,
				 TEEC_Parameter *params)
{
	uint32_t type = TEEC_NONE;
	size_t i = 0;

	for (i = 0; i < MAX_NUM_PARAMS; i++) {
		type = TEEC_PARAM_TYPE_GET(param_types, i);

		switch (type) {
		case TEEC_MEMREF_WHOLE:
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:

			if (!params[i].memref.parent)
				return TEEC_ERROR_BAD_PARAMETERS;

			if (verify_shm(params[i].memref,
				       params[i].memref.parent, ctx, type))
				return TEEC_ERROR_BAD_PARAMETERS;
			break;
		default:
			break;
		}
	}

	return TEEC_SUCCESS;
}

static TEEC_Result verify_param_types(uint32_t param_types)
{
	uint32_t type = TEEC_NONE;
	size_t i = 0;

	if (param_types & TEEC_PARAM_MASK) {
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	for (i = 0; i < MAX_NUM_PARAMS; i++) {
		type = TEEC_PARAM_TYPE_GET(param_types, i);

		switch (type) {
		case TEEC_NONE:
		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
		case TEEC_MEMREF_WHOLE:
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			break;
		default:
			return TEEC_ERROR_BAD_PARAMETERS;
		}
	}

	return TEEC_SUCCESS;
}

static int verify_conn_params(uint32_t conn_method, const void *conn_data)
{
	/* conn_method == TEEC_LOGIN_APPLICATION allowed to have data,
	 * assume data is UID populated by OEM proxy
	 */
	if ((conn_method == TEEC_LOGIN_PUBLIC ||
	     conn_method == TEEC_LOGIN_USER ||
	     conn_method == TEEC_LOGIN_USER_APPLICATION) &&
	    conn_data != NULL)
		return -1;

	if ((conn_method == TEEC_LOGIN_GROUP ||
	     conn_method == TEEC_LOGIN_GROUP_APPLICATION) &&
	    conn_data == NULL)
		return -1;

	return 0;
}

TEEC_Result TEEC_InitializeContext(const char *name, TEEC_Context *ctx)
{
	/* We always connect to Qualcomm TEE! */
	(void)name;

	if (!ctx)
		return TEEC_ERROR_BAD_PARAMETERS;

	return initialize_context(ctx);
}

void TEEC_FinalizeContext(TEEC_Context *ctx)
{
	if (!ctx)
		return;

	finalize_context(ctx);
}

TEEC_Result TEEC_OpenSession(TEEC_Context *ctx, TEEC_Session *session,
			     const TEEC_UUID *destination, uint32_t conn_method,
			     const void *connection_data, TEEC_Operation *op,
			     uint32_t *ret_origin)
{
	TEEC_Result ret = TEEC_SUCCESS;

	if (ret_origin) {
		*ret_origin = TEEC_ORIGIN_API;
	}

	if ((!ctx) || (!destination) || (!session))
		return TEEC_ERROR_BAD_PARAMETERS;

	if (verify_conn_params(conn_method, connection_data))
		return TEEC_ERROR_BAD_PARAMETERS;

	if (op) {
		if (verify_param_types(op->paramTypes))
			return TEEC_ERROR_BAD_PARAMETERS;

		if (verify_params(ctx, op->paramTypes, op->params))
			return TEEC_ERROR_BAD_PARAMETERS;
	}

	ret = open_session(ctx, session, destination, conn_method,
			   connection_data, op, ret_origin);

	return ret;
}

void TEEC_CloseSession(TEEC_Session *session)
{
	if (!session)
		return;

	close_session(session);
}

TEEC_Result TEEC_InvokeCommand(TEEC_Session *session, uint32_t command_id,
			       TEEC_Operation *op, uint32_t *ret_origin)
{
	TEEC_Result ret = TEEC_SUCCESS;
	TEEC_Context *ctx = session->imp.ctx;

	if (ret_origin) {
		*ret_origin = TEEC_ORIGIN_API;
	}

	if (!session)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (op) {
		if (verify_param_types(op->paramTypes))
			return TEEC_ERROR_BAD_PARAMETERS;

		if (verify_params(ctx, op->paramTypes, op->params))
			return TEEC_ERROR_BAD_PARAMETERS;
	}

	ret = invoke_command(session, command_id, op, ret_origin);

	return ret;
}

TEEC_Result TEEC_RegisterSharedMemory(TEEC_Context *ctx, TEEC_SharedMemory *shm)
{
	if (!ctx || !shm)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (!shm->buffer)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (!shm->flags || (shm->flags & ~(TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)))
		return TEEC_ERROR_BAD_PARAMETERS;

	if (shm->size > TEEC_CONFIG_SHAREDMEM_MAX_SIZE)
		return TEEC_ERROR_OUT_OF_MEMORY;

	return register_shared_memory(ctx, shm, FALSE);
}

TEEC_Result TEEC_AllocateSharedMemory(TEEC_Context *ctx, TEEC_SharedMemory *shm)
{
	if (!ctx || !shm)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (shm->size > TEEC_CONFIG_SHAREDMEM_MAX_SIZE)
		return TEEC_ERROR_OUT_OF_MEMORY;

	return allocate_shared_memory(ctx, shm);
}

void TEEC_ReleaseSharedMemory(TEEC_SharedMemory *shm)
{
	if (!shm)
		return;

	release_shared_memory(shm);
}

void TEEC_RequestCancellation(TEEC_Operation *op)
{
	TEEC_Session *session = NULL;

	if (!op) {
		MSGE("Invalid operation.\n");
		return;
	}

	if (op->started != 0) {
		MSGE("Operation not cancellable.\n");
		return;
	}

	session = (TEEC_Session *)op->imp.session;
	if (!session) {
		MSGE("Invalid session.\n");
		return;
	}

	request_cancellation(op);
}
