// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mink_params.h"
#include "memscpy.h"

/**
 * @brief Get the TEE_* type corresponding to a TEEC_* MemRef parameter type.
 *
 * @param teec_type The TEEC_* type defined by the Global Platform TEE Client
 *                  API specification.
 * @param op The operation payload for the request being sent.
 * @param i Index of the parameter for which conversion is required.
 * @return TEE_* type for a Memory Reference parameter.
 *         TEEC_* type for any other parameter.
 */
static uint32_t get_tee_type(uint32_t teec_type, TEEC_Operation *op, size_t i)
{
	uint32_t flags = 0;

	switch (teec_type) {
	case TEEC_MEMREF_PARTIAL_INPUT:
		return TEE_PARAM_TYPE_MEMREF_INPUT;
	case TEEC_MEMREF_PARTIAL_OUTPUT:
		return TEE_PARAM_TYPE_MEMREF_OUTPUT;
	case TEEC_MEMREF_PARTIAL_INOUT:
		return TEE_PARAM_TYPE_MEMREF_INOUT;
	case TEEC_MEMREF_WHOLE:

		flags = op->params[i].memref.parent->flags;

		if ((flags & TEEC_MEM_INPUT) && (flags & TEEC_MEM_OUTPUT))
			return TEE_PARAM_TYPE_MEMREF_INOUT;

		if (flags & TEEC_MEM_INPUT)
			return TEE_PARAM_TYPE_MEMREF_INPUT;

		if (flags & TEEC_MEM_OUTPUT)
			return TEE_PARAM_TYPE_MEMREF_OUTPUT;

		break;
	default:
		break;
	}

	return teec_type;
}

void tee_types_from_teec_types(TEEC_Operation *op, uint32_t *tee_pType)
{
	uint32_t teec_type = TEEC_NONE;
	uint32_t tee_type = TEEC_NONE;
	*tee_pType = op->paramTypes;

	for (size_t i = 0; i < MAX_NUM_PARAMS; i++) {
		teec_type = TEEC_PARAM_TYPE_GET(op->paramTypes, i);
		tee_type = get_tee_type(teec_type, op, i);

		/* In-place modification of mask */
		*tee_pType = TEEC_PARAM_TYPE_SET(tee_type, i, *tee_pType);
	}
}

void mink_params_INIT(MINK_Parameter *m_params)
{
	for (size_t i = 0; i < MAX_NUM_PARAMS; i++) {
		m_params[i].in_buf.buf = NULL;
		m_params[i].in_buf.len = 0;
		m_params[i].in_buf.sh_obj_index = 0;

		m_params[i].out_buf.buf = NULL;
		m_params[i].out_buf.len = 0;
		m_params[i].out_buf.len_out = &m_params[i].out_buf.len;

		m_params[i].mem_obj = TEEC_OBJ_NULL;
		m_params[i].mem_obj_params.offset = 0;
		m_params[i].mem_obj_params.size = 0;
		m_params[i].mem_obj_params.sharedObjIndex = 0;
	}
}

/**
 * @brief Check whether two memory objects represent the same memory.
 *
 * @param mo1 The first memory object.
 * @param mo2 The second memory object.
 * @return TRUE If the memory objects are equivalent.
 * @return FALSE If the memory objects are not equivalent.
 */
static bool is_mem_obj_equal(teec_obj_t mo1, teec_obj_t mo2)
{
	void *mo1_addr, *mo2_addr;
	size_t mo1_size, mo2_size;

	if (mink_obj_mem_info(mo1, &mo1_addr, &mo1_size))
		return false;

	if (mink_obj_mem_info(mo2, &mo2_addr, &mo2_size))
		return false;

	/* The memory represented by a memory object is mmap'd only once, hence
	 * the same memory object can never be backed by two different address
	 */
	if (((uintptr_t)mo1_addr == (uintptr_t)mo2_addr) &&
	    (mo1_size == mo2_size))
		return true;

	return false;
}

/**
 * @brief Copy the contents of Shared Memory to a Memory object represented
 *        memory.
 *
 * @param shm The Shared Memory to copy from.
 * @param mo The Memory Object to copy to.
 * @return 0 on success.
 *         -1 on failure.
 */
static int copy_to_mem_object(TEEC_SharedMemory *shm, teec_obj_t mo)
{
	void *mo_addr;
	size_t mo_size;

	if (mink_obj_mem_info(mo, &mo_addr, &mo_size))
		return -1;

	memscpy(mo_addr, mo_size, shm->buffer, shm->size);
	return 0;
}

/**
 * @brief Copy the contents of a Memory object represented memory to a Shared
 *        Memory.
 *
 * @param mo The Memory Object to copy from.
 * @param shm The Shared Memory to copy to.
 * @return 0 on success.
 *         -1 on failure.
 */
static int copy_from_mem_object(teec_obj_t mo, TEEC_SharedMemory *shm)
{
	void *mo_addr;
	size_t mo_size;

	if (mink_obj_mem_info(mo, &mo_addr, &mo_size))
		return -1;

	memscpy(shm->buffer, shm->size, mo_addr, mo_size);
	return 0;
}

/**
 * @brief Convert a MEMREF_PARTIAL_* parameter to a MEMREF_TEMP_* parameter.
 *
 * @param i Index of the parameter for which conversion is required.
 * @param param_types The parameter type encoding for the list of parameters.
 * @param params The list of parameters.
 */
static void memref_temp_from_partial(size_t i, uint32_t *param_types,
				     TEEC_Parameter *params)
{
	uint32_t type = TEEC_PARAM_TYPE_GET(*param_types, i);
	TEEC_RegisteredMemoryReference memref = params[i].memref;

	memset((void *)(&params[i]), 0, sizeof(TEEC_Parameter));
	params[i].tmpref.buffer = memref.parent->buffer;
	params[i].tmpref.size = memref.parent->size;

	free((void *)memref.parent);
	/* Convert MEMREF_PARTIAL_* to MEMREF_TEMP_* type */
	*param_types = TEEC_PARAM_TYPE_SET(type ^ 0x00000008, i, *param_types);
}

void memref_temp_from_partial_params(uint32_t *param_types,
				     TEEC_Parameter *params)
{
	uint32_t type = TEEC_NONE;
	uint8_t converted = 0;

	for (size_t i = 0; i < MAX_NUM_PARAMS; i++) {

		type = TEEC_PARAM_TYPE_GET(*param_types, i);
		switch(type) {
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:

			converted = params[i].memref.parent->imp.converted;
			if (converted)
				memref_temp_from_partial(i, param_types,
							 params);
			break;
		default:
			break;
		}
	}
}

/**
 * @brief Convert a MEMREF_TEMP_* parameter to a MEMREF_PARTIAL_* parameter.
 *
 * @param ctx The initialized TEE context.
 * @param i Index of the parameter for which conversion is required.
 * @param param_types The parameter type encoding for the list of parameters.
 * @param params The list of parameters.
 * @return TEEC_SUCCESS on success.
 *         TEEC_ERROR_* on failure.
 */
static TEEC_Result memref_temp_to_partial(TEEC_Context *ctx, size_t i,
					  uint32_t *param_types,
					  TEEC_Parameter *params)
{
	uint32_t type = TEEC_PARAM_TYPE_GET(*param_types, i);
	TEEC_TempMemoryReference tmpref = params[i].tmpref;
	size_t shm_size = sizeof(TEEC_SharedMemory);

	TEEC_SharedMemory *shm = (TEEC_SharedMemory *)malloc(shm_size);
	if (!shm)
		return TEEC_ERROR_OUT_OF_MEMORY;

	memset((void *)&params[i], 0, sizeof(TEEC_Parameter));
	params[i].memref.parent = shm;
	params[i].memref.parent->buffer = tmpref.buffer;
	params[i].memref.parent->size = tmpref.size;
	params[i].memref.parent->flags = 0;
	params[i].memref.offset = 0;
	params[i].memref.size = tmpref.size;

	if (type == TEEC_MEMREF_TEMP_INPUT ||
	    type == TEEC_MEMREF_TEMP_INOUT)
		params[i].memref.parent->flags |= TEEC_MEM_INPUT;

	if (type == TEEC_MEMREF_TEMP_OUTPUT ||
	    type == TEEC_MEMREF_TEMP_INOUT)
		params[i].memref.parent->flags |= TEEC_MEM_OUTPUT;

	/* Convert MEMREF_TEMP_* to MEMREF_PARTIAL_* type */
	*param_types = TEEC_PARAM_TYPE_SET(type | 0x00000008, i, *param_types);

	return register_shared_memory(ctx, params[i].memref.parent, TRUE);
}

TEEC_Result memref_temp_to_partial_params(TEEC_Context *ctx,
					  uint32_t *param_types,
					  TEEC_Parameter *params)
{
	TEEC_Result result = TEEC_SUCCESS;
	uint32_t type = TEEC_NONE;
	size_t size = 0;

	for (size_t i = 0; i < MAX_NUM_PARAMS; i++) {

		type = TEEC_PARAM_TYPE_GET(*param_types, i);
		switch(type) {
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:

			size = params[i].tmpref.size;
			if (size > TEEC_SHM_MAX_HEAP_SZ) {
				result = memref_temp_to_partial(ctx, i,
								param_types,
								params);
				if (result)
					goto out_failed;
			}

			break;
		default:
			break;
		}
	}

	return result;

out_failed:
	/* Undo the conversion of TEMP params done until this point */
	memref_temp_from_partial_params(param_types, params);

	return result;
}

void update_shm_memref_from_mem_obj(uint32_t param_types,
				    TEEC_Parameter *params)
{
	uint32_t type = TEEC_NONE;
	TEEC_SharedMemory *shm;
	teec_obj_t mem_obj;

	for (size_t i = 0; i < MAX_NUM_PARAMS; i++) {

		type = TEEC_PARAM_TYPE_GET(param_types, i);
		switch(type) {
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
		case TEEC_MEMREF_WHOLE:

			shm = params[i].memref.parent;
			mem_obj = shm->imp.mem_obj;
			if (!TEEC_OBJ_IS_NULL(mem_obj) &&
			    shm->imp.type == TEEC_MEMORY_REGISTERED)
				copy_from_mem_object(mem_obj, shm);
			break;
		default:
			break;
		}
	}
}

/**
 * @brief Convert a TEEC_VALUE_* parameter to a MINK parameter.
 *
 * @param i Index of the parameter for which conversion is required.
 * @param param_types The parameter type encoding for the list of parameters.
 * @param params The list of TEEC parameters.
 * @param m_params The list of MINK parameters to be assigned.
 */
static void process_value_param(size_t i, uint32_t param_types,
				TEEC_Parameter *params,
				MINK_Parameter *m_params)
{
	MINK_InBuffer *inbuf = &m_params[i].in_buf;
	MINK_OutBuffer *outbuf = &m_params[i].out_buf;

	uint32_t type = TEEC_PARAM_TYPE_GET(param_types, i);

	if (type == TEEC_VALUE_INPUT ||
	    type == TEEC_VALUE_INOUT) {
		inbuf->buf = &params[i].value;
		inbuf->len =  sizeof(TEEC_Value);
		inbuf->sh_obj_index = 0xFF; // N/A
	}

	if (type == TEEC_VALUE_OUTPUT ||
	    type == TEEC_VALUE_INOUT) {
		outbuf->buf = &params[i].value;
		outbuf->len =  sizeof(TEEC_Value);
	}
}

/**
 * @brief Convert a TEEC_MEMREF_TEMP_* parameter to a MINK parameter.
 *
 * @param i Index of the parameter for which conversion is required.
 * @param param_types The parameter type encoding for the list of parameters.
 * @param params The list of TEEC parameters.
 * @param m_params The list of MINK parameters to be assigned.
 * @param etype The parameter type encoding for extended parameters
 *              in this request for use by QTEE.
 */
static void process_memref_temp(size_t i, uint32_t param_types,
				TEEC_Parameter *params,
				MINK_Parameter *m_params,
				uint32_t *etype)
{
	uint32_t type = TEEC_PARAM_TYPE_GET(param_types, i);
	MINK_InBuffer *inbuf = &m_params[i].in_buf;
	MINK_OutBuffer *outbuf = &m_params[i].out_buf;

	/* Implicitly handles NULL tmpref */
	inbuf->buf = params[i].tmpref.buffer;
	inbuf->len = params[i].tmpref.size;
	inbuf->sh_obj_index = 0;

	if (type == TEEC_MEMREF_TEMP_OUTPUT ||
	    type == TEEC_MEMREF_TEMP_INOUT) {
		outbuf->buf = params[i].tmpref.buffer;
		outbuf->len = params[i].tmpref.size;
	}
	outbuf->len_out = &params[i].tmpref.size;

	if (params[i].tmpref.buffer == NULL)
		*etype = TEEC_PARAM_TYPE_SET(MINK_EX_PARAM_TYPE_MEMREF_NULL, i,
					     *etype);
}

/**
 * @brief Get the index of the first TEEC_MEMREF_* parameter which shares a
 * Memory object with the current TEEC_MEMREF_* parameter.
 *
 * @param memref_index Index of the current TEEC_MEMREF_* parameter.
 * @param memref_mem_obj The Memory object for the current parameter.
 * @param param_types The parameter type encoding for the list of parameters.
 * @param params The list of parameters.
 * @return i Index of the first shared TEEC_MEMREF_* parameter.
 *         DEFINING_INDEX_NA otherwise.
 */
static size_t get_shared_mem_obj_index(size_t memref_index,
				       teec_obj_t memref_mem_obj,
				       uint32_t param_types,
				       TEEC_Parameter *params)
{
	uint32_t type = TEEC_NONE;
	TEEC_SharedMemory *shm;
	teec_obj_t mem_obj;
	for (size_t i = 0; i < memref_index; i++) {

		type = TEEC_PARAM_TYPE_GET(param_types, i);
		switch(type) {
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
		case TEEC_MEMREF_WHOLE:

			shm = params[i].memref.parent;
			mem_obj = shm->imp.mem_obj;
			if (!TEEC_OBJ_IS_NULL(mem_obj) &&
			    is_mem_obj_equal(mem_obj, memref_mem_obj))

				return i;
			break;
		default:
			break;
		}
	}

	return DEFINING_INDEX_NA;
}

/**
 * @brief Assign extended parameters for the TEEC_MEMREF_* parameters sharing
 *        Memory objects.
 *
 * @param index Index of the current TEEC_MEMREF_* parameter.
 * @param shm_index Index of the first TEEC_MEMREF_* parameter which shares a
 *                  Memory object with the current TEEC_MEMREF_* parameter.
 * @param param_type The type of the current TEEC_MEMREF_* parameter.
 * @param tee_exParamTypes The parameter type encoding for extended parameters
 *                         in this request for use by QTEE.
 */
static void assign_extended_params(size_t index, int shm_index,
				   uint32_t param_type,
				   uint32_t *tee_exParamTypes)
{
	uint32_t etype = 0;
	/* The parameter at 'index' shares a memory object with some other
	 * parameter.
	 */
	etype = TEEC_PARAM_TYPE_SET(MINK_EX_PARAM_TYPE_MEMREF_DUP, index,
				    *tee_exParamTypes);

	/* Inform QTEE that the parameter at 'shm_index' is the one hosting
	 * the memory object shared by the parameter at 'index'.
	 */
	if (param_type == TEEC_MEMREF_PARTIAL_OUTPUT ||
	    param_type == TEEC_MEMREF_PARTIAL_INOUT)
		etype = TEEC_PARAM_TYPE_SET(MINK_EX_PARAM_TYPE_MEMREF_FORCE_RW,
					    shm_index, etype);

	*tee_exParamTypes = etype;
}

/**
 * @brief Convert a TEEC_MEMREF_WHOLE parameter to a MINK parameter.
 *
 * @param i Index of the parameter for which conversion is required.
 * @param param_types The parameter type encoding for the list of parameters.
 * @param params The list of parameters.
 * @param m_params The list of MINK parameters to be assigned.
 * @param tee_exParamTypes The parameter type encoding for extended parameters
 *                         in this request for use by QTEE.
 */
static void process_memref_whole(size_t i, uint32_t param_types,
				 TEEC_Parameter *params,
				 MINK_Parameter *m_params,
				 uint32_t *tee_exParamTypes)
{
	uint32_t type = TEEC_PARAM_TYPE_GET(param_types, i);
	TEEC_SharedMemory *shm = params[i].memref.parent;
	teec_obj_t memref_mem_obj = shm->imp.mem_obj;
	size_t shm_obj_index = 0;

	/* Pointers to MINK parameters to be assigned */
	MINK_InBuffer *inbuf = &m_params[i].in_buf;
	MINK_OutBuffer *outbuf = &m_params[i].out_buf;
	teec_obj_t *mem_obj = &m_params[i].mem_obj;
	mink_mem_params *mem_obj_params = &m_params[i].mem_obj_params;

	/* This whole memory reference is backed by a memory object */
	if (!TEEC_OBJ_IS_NULL(memref_mem_obj)) {

		/* We need to set the memory object and it's parameters */
		*mem_obj = memref_mem_obj;
		mem_obj_params->offset = 0;
		mem_obj_params->size = params[i].memref.parent->size;

		/* In case of TEEC_MEMORY_ALLOCATED, shm->buffer already
		 * points to memory object backed memory, thus no need for copy.
		 */
		if (shm->imp.type == TEEC_MEMORY_REGISTERED)
			copy_to_mem_object(shm, memref_mem_obj);

		inbuf->buf = mem_obj_params;
		inbuf->len = sizeof(*mem_obj_params);

		/* Does this memory reference share it's memory object with
		 * another memory reference? */
		shm_obj_index = get_shared_mem_obj_index(i, memref_mem_obj,
							 param_types,
							 params);

		if (shm_obj_index != DEFINING_INDEX_NA)
			assign_extended_params(i, shm_obj_index, type,
					       tee_exParamTypes);

		inbuf->sh_obj_index = shm_obj_index;

		if (params[i].memref.parent->flags & TEEC_MEM_OUTPUT)
			outbuf->len_out = &params[i].memref.size;
	} else {
		inbuf->buf = params[i].memref.parent->buffer;
		inbuf->len = params[i].memref.parent->size;
		inbuf->sh_obj_index = 0;

		if (params[i].memref.parent->flags & TEEC_MEM_OUTPUT) {
			outbuf->buf = params[i].memref.parent->buffer;
			outbuf->len = params[i].memref.parent->size;
			/* As per the GP spec, even if type is MEMREF_WHOLE,
			 * we must update the size here
			 */
			outbuf->len_out = &params[i].memref.size;
		}
	}
}

/**
 * @brief Convert a TEEC_MEMREF_PARTIAL_* parameter to a MINK parameter.
 *
 * @param i Index of the parameter for which conversion is required.
 * @param param_types The parameter type encoding for the list of parameters.
 * @param params The list of parameters.
 * @param m_params The list of MINK parameters to be assigned.
 * @param tee_exParamTypes The parameter type encoding for extended parameters
 *                         in this request for use by QTEE.
 */
static void process_memref_partial(size_t i, uint32_t param_types,
				   TEEC_Parameter *params,
				   MINK_Parameter *m_params,
				   uint32_t *tee_exParamTypes)
{
	uint32_t type = TEEC_PARAM_TYPE_GET(param_types, i);
	TEEC_SharedMemory *shm = params[i].memref.parent;
	teec_obj_t memref_mem_obj = shm->imp.mem_obj;
	size_t shm_obj_index = 0;

	/* Pointers to MINK parameters to be assigned */
	MINK_InBuffer *inbuf = &m_params[i].in_buf;
	MINK_OutBuffer *outbuf = &m_params[i].out_buf;
	teec_obj_t *mem_obj = &m_params[i].mem_obj;
	mink_mem_params *mem_obj_params = &m_params[i].mem_obj_params;

	/* This partial memory reference is backed by a memory object */
	if (!TEEC_OBJ_IS_NULL(memref_mem_obj)) {

		/* We need to set the memory object and it's parameters */
		*mem_obj = memref_mem_obj;
		mem_obj_params->offset = params[i].memref.offset;
		mem_obj_params->size = params[i].memref.size;

		/* In case of TEEC_MEMORY_ALLOCATED, shm->buffer already
		 * points to memory object backed memory, thus no need for copy.
		 */
		if (shm->imp.type == TEEC_MEMORY_REGISTERED)
			copy_to_mem_object(shm, memref_mem_obj);

		inbuf->buf = mem_obj_params;
		inbuf->len = sizeof(*mem_obj_params);

		/* Does this memory reference share it's memory object with
		 * another memory reference? */
		shm_obj_index = get_shared_mem_obj_index(i, memref_mem_obj,
							 param_types,
							 params);

		if (shm_obj_index != DEFINING_INDEX_NA)
			assign_extended_params(i, shm_obj_index, type,
					       tee_exParamTypes);

		inbuf->sh_obj_index = shm_obj_index;

		if (type == TEEC_MEMREF_PARTIAL_OUTPUT ||
		    type == TEEC_MEMREF_PARTIAL_INOUT)
			outbuf->len_out = &params[i].memref.size;
	} else {
		inbuf->buf = params[i].memref.parent->buffer
			     + params[i].memref.offset;
		inbuf->len = params[i].memref.size;
		inbuf->sh_obj_index = 0;

		if (type == TEEC_MEMREF_PARTIAL_OUTPUT ||
		    type == TEEC_MEMREF_PARTIAL_INOUT) {
			outbuf->buf = params[i].memref.parent->buffer
				      + params[i].memref.offset;
			outbuf->len = params[i].memref.size;
			outbuf->len_out = &params[i].memref.size;
		}
	}
}

void mink_params_from_teec_params(uint32_t param_types, TEEC_Parameter *params,
				MINK_Parameter *m_params,
				uint32_t *tee_exParamTypes)
{
	uint32_t type = TEEC_NONE;

	for (size_t i = 0; i < MAX_NUM_PARAMS; i++) {

		type = TEEC_PARAM_TYPE_GET(param_types, i);
		switch(type) {
		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:

			process_value_param(i, param_types, params, m_params);
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:

			process_memref_temp(i, param_types, params, m_params,
					    tee_exParamTypes);
			break;
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:

			process_memref_partial(i, param_types, params, m_params,
					       tee_exParamTypes);

			break;
		case TEEC_MEMREF_WHOLE:

			process_memref_whole(i, param_types, params, m_params,
					     tee_exParamTypes);
			break;
		default:
			break;
		}
	}
}
