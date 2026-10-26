// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <string.h>

#include "mink_adaptor_priv.h"
#include "supplicant.h"
#include "IRequestTABuffer_invoke.h"
#include "IRequestTABuffer.h"

#include "MinkCom.h"

static int invoke_over_tee(ObjectCxt cxt, ObjectOp op, ObjectArg *args,
			   ObjectCounts counts);

static qcomtee_result_t
qcomtee_callback_obj_dispatch(struct qcomtee_object *object, qcomtee_op_t op,
			      struct qcomtee_param *params, int num);

static void qcomtee_callback_obj_cleanup(struct qcomtee_object *object,
					 int err);

static void qcomtee_callback_obj_release(struct qcomtee_object *object);

static struct qcomtee_object_ops ops = {
	.dispatch = qcomtee_callback_obj_dispatch,
	.error = qcomtee_callback_obj_cleanup,
	.release = qcomtee_callback_obj_release,
};

/**
 * @brief Get a QCOMTEE object from a MINK object.
 *
 * @param root_object The root object associated with this invocation.
 * @param obj The MINK object to be converted to a QCOMTEE object.
 * @param object The QCOMTEE object converted from the MINK object.
 * @return Returns Object_OK object on success.
 *         Returns Object_ERROR_* on failure.
 */
static int32_t
qcomtee_obj_from_mink_obj(struct qcomtee_object *root_object, Object obj,
			  struct qcomtee_object **object)
{
	if (Object_isNull(obj)) {
		*object = QCOMTEE_OBJECT_NULL;
		return Object_OK;
	} else if (obj.invoke == invoke_over_tee) {
		*object = (struct qcomtee_object *)obj.context;
		return Object_OK;
	} else {
		struct qcomtee_callback_obj *qcomtee_cbo;

		qcomtee_cbo = calloc(1, sizeof(*qcomtee_cbo));
		if (!qcomtee_cbo)
			return Object_ERROR_MEM;

		qcomtee_object_cb_init(&qcomtee_cbo->object, &ops, root_object);

		/* Only support sharing a copy of the callback object with QTEE,
		 * increase the refcout.
		 */
		Object_retain(obj);

		qcomtee_cbo->mink_obj = obj;
		*object = &qcomtee_cbo->object;
		return Object_OK;
	}
}

/**
 * @brief Get a MINK object from a QCOMTEE object.
 *
 * @param qcomtee_obj The QCOMTEE object to be converted to a MINK object.
 * @return Returns a MINK Object on success.
 *         Returns Object_NULL on failure.
 */
static Object mink_obj_from_qcomtee_obj(struct qcomtee_object *qcomtee_obj)
{
	if (qcomtee_obj == QCOMTEE_OBJECT_NULL) {
		return Object_NULL;
	} else if (qcomtee_object_typeof(qcomtee_obj) == QCOMTEE_OBJECT_TYPE_CB) {
		struct qcomtee_callback_obj *qcomtee_cbo =
			CALLBACKOBJ(qcomtee_obj);
		return qcomtee_cbo->mink_obj;
	} else { /* Remote and Memory Objects */
		return (Object){ invoke_over_tee, qcomtee_obj };
	}
}

/**
 * @brief Generate a mask for encoding the number and type of parameters.
 *
 * @param params List of QCOMTEE parameters.
 * @param num_params Number of parameters in the QCOMTEE parameters list.
 * @return Returns an ObjectCounts mask encoding the number and type of
 *         the parameters.
 */
static ObjectCounts get_obj_counts(struct qcomtee_param *params,
				   uint32_t num_params)
{
	uint32_t bi = 0, bo = 0, oi = 0, oo = 0;

	for (uint32_t i = 0; i < num_params; i++) {
		switch (params[i].attr) {
		case QCOMTEE_UBUF_INPUT:
			bi++;
			break;
		case QCOMTEE_UBUF_OUTPUT:
			bo++;
			break;
		case QCOMTEE_OBJREF_INPUT:
			oi++;
			break;
		case QCOMTEE_OBJREF_OUTPUT:
			oo++;
			break;
		default:
			return Object_ERROR_INVALID;
		}
	}

	return ObjectCounts_pack(bi, bo, oi, oo);
}

/**
 * @brief Release QCOMTEE objects in the event of marshalling failure.
 *
 * @param params List of QCOMTEE parameters.
 * @param num Number of parameters in the params list.
 * @param attr Type of QCOMTEE objects to release QCOMTEE_OBJREF_INPUT
 *             or QCOMTEE_OBJREF_OUTPUT.
 */
static void release_qcomtee_objs(struct qcomtee_param *params,
				 uint32_t num_params, uint64_t attr)
{
	if (attr != QCOMTEE_OBJREF_INPUT && attr != QCOMTEE_OBJREF_OUTPUT)
		return;

	for (uint32_t i = 0; i < num_params; i++) {
		if (params[i].attr == attr)
			qcomtee_object_refs_dec(params[i].object);
	}
}

/**
 * @brief Convert QCOMTEE parameters to MINK arguments during callback
 * processing.
 *
 * @param params List of QCOMTEE parameters.
 * @param num_params Number of parameters in the QCOMTEE parameters list.
 * @param args List of MINK arguments.
 * @return Object_OK on success.
 *         OBJECT_ERROR_INVALID on failure.
 */
static int32_t object_args_from_tee_params_cb(struct qcomtee_param *params,
					      uint32_t num_params,
					      ObjectArg *args,
					      void **allocated_bo)
{
	uint32_t bo = 0;
	uint32_t i = 0;
	void *ubuf_ptr = NULL;

	for (i = 0; i < num_params; i++) {
		switch (params[i].attr) {
		case QCOMTEE_UBUF_INPUT:
			args[i].b.ptr = params[i].ubuf.addr;
			args[i].b.size = params[i].ubuf.size;
			break;
		case QCOMTEE_UBUF_OUTPUT:
			/* params[i].ubuf.addr is NULL for UBUF_OUTPUT */
			ubuf_ptr = malloc(params[i].ubuf.size);
			if (!ubuf_ptr)
				goto out_failed;

			args[i].b.ptr = ubuf_ptr;
			args[i].b.size = params[i].ubuf.size;

			/* Account for it, we'll free it later during cleanup */
			allocated_bo[bo++] = args[i].b.ptr;
			break;
		case QCOMTEE_OBJREF_INPUT:
			args[i].o = mink_obj_from_qcomtee_obj(params[i].object);
			break;
		case QCOMTEE_OBJREF_OUTPUT:
			break;
		default:
			return Object_ERROR_INVALID;
		}
	}

	return Object_OK;

out_failed:
	for (i = 0; i < bo; i++) {
		if (allocated_bo[i])
			free(allocated_bo[i]);
	}

	return Object_ERROR;
}

/**
 * @brief Convert MINK arguments to QCOMTEE parameters during callback
 * processing.
 *
 * @param args List of MINK arguments.
 * @param counts Mask encoding the number and type of arguments in args.
 * @param params List of QCOMTEE parameters.
 * @param root_object The root object associated with this invocation.
 * @return Object_OK on success.
 *         Object_ERROR on failure.
 */
static int32_t object_args_to_tee_params_cb(ObjectArg *args,
					    ObjectCounts counts,
					    struct qcomtee_param *params,
					    struct qcomtee_object *root)
{
	FOR_ARGS(i, counts, BI) {
		params[i].attr = QCOMTEE_UBUF_INPUT;
		params[i].ubuf.addr = args[i].b.ptr;
		params[i].ubuf.size = args[i].b.size;
	}

	FOR_ARGS(i, counts, BO) {
		params[i].attr = QCOMTEE_UBUF_OUTPUT;
		params[i].ubuf.addr = args[i].b.ptr;
		params[i].ubuf.size = args[i].b.size;
	}

	FOR_ARGS(i, counts, OI) {
		params[i].attr = QCOMTEE_OBJREF_INPUT;
	}

	FOR_ARGS(i, counts, OO) {
		params[i].attr = QCOMTEE_OBJREF_OUTPUT;
		if(qcomtee_obj_from_mink_obj(root, args[i].o, &params[i].object))
			goto out_failed;
	}

	return Object_OK;

out_failed:
	return Object_ERROR;
}

/**
 * @brief Dispatch a request to the MINK callback object.
 *
 * Forwards the dispatch request received from QCOMTEE to the MINK defined
 * callback object after converting the parameters of the call to MINK format.
 *
 * @param object The QCOMTEE callback object being invoked.
 * @param op Operation being invoked on the callback object.
 * @param params List of parameters for this invocation in  QCOMTEE format.
 * @param num Number of parameters in the params list.
 * @return Object_OK/QCOMTEE_OK on success.
 *         qcomtee_result_t on failure.
 */
static qcomtee_result_t
qcomtee_callback_obj_dispatch(struct qcomtee_object *object, qcomtee_op_t op,
			      struct qcomtee_param *params, int num)
{
	qcomtee_result_t ret = QCOMTEE_OK;

	struct qcomtee_object *root = object->root;
	struct qcomtee_callback_obj *qcomtee_cbo = CALLBACKOBJ(object);

	ObjectArg objArgs[MAX_OBJ_ARG_COUNT] = { { { 0, 0 } } };
	ObjectCounts counts = get_obj_counts(params, num);

	ret = object_args_from_tee_params_cb(params, num, objArgs,
					     qcomtee_cbo->allocated_bo);
	if (ret)
		return ret;

	ret = Object_invoke(qcomtee_cbo->mink_obj, op, objArgs, counts);
	if (!ret) {
		ret = object_args_to_tee_params_cb(objArgs, counts, params,
						   root);
		if (ret) {
			release_qcomtee_objs(params, num,
					     QCOMTEE_OBJREF_OUTPUT);

			/* If dispatch fails, QCOMTEE doesn't call cleanup */
			qcomtee_callback_obj_cleanup(object, 0);
		}
	}

	return ret;
}

/**
 * @brief Clean up resources after sending the response for the dispatch
 * request to QCOMTEE.
 *
 * Invoked by QCOMTEE post-response to allow the Mink Adaptor layer to
 * release any resources allocated during the dispatch request.
 *
 * @param object The QCOMTEE callback object which was invoked.
 * @param err Error (if any) encountered while sending the response to QCOMTEE.
 */
static void qcomtee_callback_obj_cleanup(struct qcomtee_object *object, int err)
{
	(void)err;
	struct qcomtee_callback_obj *qcomtee_cbo = CALLBACKOBJ(object);
	uint32_t i = 0;

	IRequestTABuffer_cleanup(qcomtee_cbo->mink_obj);
	for (i = 0; i < ObjectCounts_maxBO; i++) {
		if (qcomtee_cbo->allocated_bo[i])
			free(qcomtee_cbo->allocated_bo[i]);
	}
}

/**
 * @brief Release a MINK callback object.
 *
 * Forwards the release request received from QCOMTEE to the MINK defined
 * callback object.
 *
 * @param object The QCOMTEE callback object being released.
 */
static void qcomtee_callback_obj_release(struct qcomtee_object *object)
{
	struct qcomtee_callback_obj *qcomtee_cbo = CALLBACKOBJ(object);

	Object_release(qcomtee_cbo->mink_obj);
	free(qcomtee_cbo);
}

/**
 * @brief Convert MINK arguments to QCOMTEE parameters.
 *
 * @param args List of MINK arguments.
 * @param counts Mask encoding the number and type of arguments in args.
 * @param params List of QCOMTEE parameters.
 * @param root The root object associated with this invocation.
 * @return Object_OK on success.
 *         Object_ERROR on failure.
 */
static int32_t object_args_to_tee_params(ObjectArg *args, ObjectCounts counts,
					 struct qcomtee_param *params,
					 struct qcomtee_object *root)
{
	FOR_ARGS(i, counts, BI) {
		params[i].attr = QCOMTEE_UBUF_INPUT;
		params[i].ubuf.addr = args[i].b.ptr;
		params[i].ubuf.size = args[i].b.size;
	}

	FOR_ARGS(i, counts, BO) {
		params[i].attr = QCOMTEE_UBUF_OUTPUT;
		params[i].ubuf.addr = args[i].b.ptr;
		params[i].ubuf.size = args[i].b.size;
	}

	FOR_ARGS(i, counts, OI) {
		params[i].attr = QCOMTEE_OBJREF_INPUT;
		if(qcomtee_obj_from_mink_obj(root, args[i].o, &params[i].object))
			goto out_failed;
	}

	FOR_ARGS(i, counts, OO) {
		params[i].attr = QCOMTEE_OBJREF_OUTPUT;
	}

	return Object_OK;

out_failed:
	return Object_ERROR;
}

/**
 * @brief Convert QCOMTEE parameters to MINK arguments.
 *
 * @param params List of QCOMTEE parameters.
 * @param args List of MINK arguments.
 * @param counts Mask encoding the number and type of arguments in args.
 * @return Object_OK on success.
 *         OBJECT_ERROR_INVALID on failure.
 */
static int32_t object_args_from_tee_params(struct qcomtee_param *params,
					   ObjectArg *args, ObjectCounts counts)
{
	uint32_t num_params = ObjectCounts_total(counts);

	for (uint32_t i = 0; i < num_params; i++) {
		switch (params[i].attr) {
		case QCOMTEE_UBUF_INPUT:
			args[i].b.ptr = params[i].ubuf.addr;
			args[i].b.size = params[i].ubuf.size;
			break;
		case QCOMTEE_UBUF_OUTPUT:
			args[i].b.ptr = params[i].ubuf.addr;
			args[i].b.size = params[i].ubuf.size;
			break;
		case QCOMTEE_OBJREF_INPUT:
			break;
		case QCOMTEE_OBJREF_OUTPUT:
			args[i].o = mink_obj_from_qcomtee_obj(params[i].object);
			break;
		default:
			return Object_ERROR_INVALID;
		}
	}

	return Object_OK;
}

/**
 * @brief Invoke an object in TEE.
 *
 * @param ctx Object context.
 * @param op Operation being requested from TEE. List of MINK arguments.
 * @param args List of MINK arguments.
 * @param counts Mask encoding the number and type of arguments in args.
 * @return Object_OK on success.
 *         Object_ERROR_* on failure.
 */
static int invoke_over_tee(ObjectCxt cxt, ObjectOp op, ObjectArg *args,
			   ObjectCounts counts)
{
	int ret = Object_OK;
	struct qcomtee_param params[ObjectCounts_total(counts)];
	qcomtee_result_t result;

	struct qcomtee_object *object = (struct qcomtee_object *)cxt;
	if (object == NULL) {
		MSGE("cxt is null");
		return Object_ERROR_BADOBJ;
	}

	ObjectOp method = ObjectOp_methodID(op);
	if (ObjectOp_isLocal(op)) {
		switch (method) {
		case Object_OP_retain:

			qcomtee_object_refs_inc(object);
			return Object_OK;
		case Object_OP_release:

			qcomtee_object_refs_dec(object);
			return Object_OK;
		default:
			return Object_ERROR_REMOTE;
		}
	}

	ret = object_args_to_tee_params(args, counts, params, object->root);
	if (ret)
		goto err_marshal_in;

	if (qcomtee_object_invoke(object, op, params,
				  ObjectCounts_total(counts), &result)) {
		MSGE("Failed qcomtee_object_invoke\n");
		ret = Object_ERROR;
		goto err_marshal_in;
	}

	if (result) {
		MSGD("Failed qcomtee_object_invoke. result = 0x%x\n", result);
		ret = result;
		goto err_result;
	}

	ret = object_args_from_tee_params(params, args, counts);

err_result:
	/* qcomtee_object_invoke was successful; QTEE releases OI. */

	return ret;

err_marshal_in:
	release_qcomtee_objs(params, ObjectCounts_total(counts),
			     QCOMTEE_OBJREF_INPUT);

	return ret;
}

int MinkCom_getRootEnvObject(Object *obj)
{
	struct supplicant *sup = supplicant_start(DEFAULT_CBOBJ_THREAD_CNT);
	if (!sup) {
		MSGE("Failed supplicant_start\n");
		return Object_ERROR;
	}

	*obj = mink_obj_from_qcomtee_obj(sup->root);
	return Object_OK;
}

int MinkCom_getClientEnvObject(Object rootObj, Object *clientEnvObj)
{
	int ret = Object_OK;

	struct qcomtee_object *root = (struct qcomtee_object *)rootObj.context;
	struct qcomtee_object *creds_object;
	struct qcomtee_param params[2];
	qcomtee_result_t result;

	if (qcomtee_object_credentials_init(root, &creds_object)) {
		MSGE("Failed qcomtee_object_credentials_init\n");
		return Object_ERROR;
	}

	params[0].attr = QCOMTEE_OBJREF_INPUT;
	params[0].object = creds_object;
	params[1].attr = QCOMTEE_OBJREF_OUTPUT;

	/* 2 is IClientEnv_OP_registerAsClient. */
	if (qcomtee_object_invoke(root, 2, params, 2, &result)) {
		ret = Object_ERROR;
		MSGE("Failed qcomtee_object_invoke\n");
		goto err_object_invoke;
	}

	if (result) {
		MSGE("Failed qcomtee_object_invoke. result = 0x%x\n", result);
		ret = result;
		goto err_result;
	}

	*clientEnvObj = mink_obj_from_qcomtee_obj(params[1].object);

err_result:
	/* qcomtee_object_invoke was successful; QTEE releases creds_object. */

	return ret;

err_object_invoke:
	qcomtee_object_refs_dec(creds_object);

	return ret;
}

int MinkCom_getClientEnvObjectWithCreds(Object rootObj, Object creds,
					Object *obj)
{
	int ret = Object_OK;

	struct qcomtee_object *root = (struct qcomtee_object *)rootObj.context;
	struct qcomtee_object *creds_object;
	struct qcomtee_param params[2];
	qcomtee_result_t result;

	ret = qcomtee_obj_from_mink_obj(root, creds, &creds_object);
	if (ret)
		return ret;

	params[0].attr = QCOMTEE_OBJREF_INPUT;
	params[0].object = creds_object;
	params[1].attr = QCOMTEE_OBJREF_OUTPUT;

	/* 5 is IClientEnv_OP_registerWithCredentials. */
	if (qcomtee_object_invoke(root, 2, params, 2, &result)) {
		ret = Object_ERROR;
		MSGE("Failed qcomtee_object_invoke\n");
		goto err_object_invoke;
	}

	if (result) {
		MSGE("Failed qcomtee_object_invoke. result = 0x%x\n", result);
		ret = result;
		goto err_result;
	}

	*obj = mink_obj_from_qcomtee_obj(params[1].object);

err_result:
	/* qcomtee_object_invoke was successful; QTEE releases creds_object. */

	return ret;

err_object_invoke:
	qcomtee_object_refs_dec(creds_object);

	return ret;
}

int MinkCom_getMemoryObject(Object rootObj, size_t size, Object *memObj)
{
	int ret = Object_OK;

	struct qcomtee_object *memory_object = NULL;
	struct qcomtee_object *root = (struct qcomtee_object *)rootObj.context;
	if (!root) {
		ret = Object_ERROR;
		goto err;
	}

	if(qcomtee_memory_object_alloc(size, root, &memory_object)) {
		ret = Object_ERROR;
		goto err;
	}

	*memObj = mink_obj_from_qcomtee_obj(memory_object);
err:
	return ret;
}

int MinkCom_getMemoryObjectInfo(Object memObj, void **address, size_t *size)
{
	int ret = Object_OK;

	struct qcomtee_object *memory_object = NULL;

	memory_object = (struct qcomtee_object *)memObj.context;
	if (!memory_object) {
		ret = Object_ERROR;
		goto err;
	}

	if (qcomtee_object_typeof(memory_object) !=
					QCOMTEE_OBJECT_TYPE_MEMORY) {
		ret = Object_ERROR;
		goto err;
	}

	*address = qcomtee_memory_object_addr(memory_object);
	*size = qcomtee_memory_object_size(memory_object);

	if (!*address || !*size) {
		ret = Object_ERROR;
		goto err;
	}

err:
	return ret;
}
