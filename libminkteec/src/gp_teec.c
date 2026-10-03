// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/* Direct libqcomtee backend.
 *
 * Implements the nine entry points declared in gp_teec.h on top of libqcomtee,
 * without going through libminkadaptor or the idlc generated stubs. The
 * counterpart for the default build is mink_teec.c; exactly one of the two is
 * compiled, selected by MINKTEEC_DIRECT_QCOMTEE.
 *
 * Everything this file knows about QTEE's wire format comes from gp_teec.h.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gp_teec.h"

/* The public TEEC_Context carries nothing but the three handles this backend
 * fills in, so the assert catches a field being added to tee_client_api.h
 * without gp_teec.c learning about it.
 */
_Static_assert(sizeof(TEEC_Context) == 3 * sizeof(teec_obj_t),
	       "TEEC_Context layout drift");

/**
 * @brief Report whether handing an object to QTEE consumes a reference.
 *
 * QTEE takes ownership of the reference for the object classes it has to keep
 * alive on its own: callback objects, which it may invoke back at any time,
 * and memory objects, which it maps. For objects that live in QTEE already
 * (QCOMTEE_OBJECT_TYPE_TEE) or for the root, passing the handle transfers
 * nothing. A NULL slot is answered with 0, since qcomtee_object_typeof()
 * maps QCOMTEE_OBJECT_NULL to QCOMTEE_OBJECT_TYPE_NULL.
 *
 * @param o The object about to be placed in an input slot.
 * @return Non-zero if a reference has to be added on behalf of QTEE.
 */
static inline int oi_transfers_ref(struct qcomtee_object *o)
{
	qcomtee_object_type_t t = qcomtee_object_typeof(o);

	return t == QCOMTEE_OBJECT_TYPE_CB || t == QCOMTEE_OBJECT_TYPE_MEMORY;
}

/**
 * @brief Place a borrowed object in an input slot.
 *
 * The caller keeps its own reference; the one added here belongs to QTEE and
 * is consumed by a successful qcomtee_object_invoke(). Objects the caller
 * means to hand over outright must not go through this helper: fill the slot
 * directly and release on failure instead.
 *
 * @param p The slot to fill.
 * @param o The object to place in it, possibly TEEC_OBJ_NULL.
 */
static inline void oi_fill(struct qcomtee_param *p, struct qcomtee_object *o)
{
	p->attr = QCOMTEE_OBJREF_INPUT;
	p->object = o;

	if (oi_transfers_ref(o))
		qcomtee_object_refs_inc(o);
}

/**
 * @brief Give back the references oi_fill() added.
 *
 * Only ever correct after a transport level failure, that is when
 * qcomtee_object_invoke() itself returned non-zero and the request never
 * reached QTEE. Once QTEE has seen the request it owns those references even
 * if the method result is an error, so calling this on a non-zero result
 * double frees.
 *
 * @param p The slot array.
 * @param first Index of the first input object slot to unwind.
 * @param n Number of slots to unwind.
 */
static void oi_rollback(struct qcomtee_param *p, int first, int n)
{
	for (int i = first; i < first + n; i++)
		if (oi_transfers_ref(p[i].object))
			qcomtee_object_refs_dec(p[i].object);
}

/**
 * @brief Marshal and issue an openSession or invokeCommand request.
 *
 * The two methods differ only in which slots exist, so both are expressed as
 * one call: the slot indices are passed in, and the three that only
 * openSession has (@p bi_uuid, @p oi_waiter, @p oo_session) may be -1 to say
 * "no such slot". @p bi_scalars, @p bo_scalars, @p bi_param0, @p bo_param0
 * and @p oi_param0 exist in both methods and must be valid; they are not
 * checked, and a negative value writes below the slot array.
 *
 * On return the memref sizes reported by QTEE have already been written back
 * through MINK_Parameter::out_buf::len_out, which is why mink_params_INIT() has to
 * have run over @p call_param.
 *
 * @param target The object to invoke: the app client for openSession, the
 *               session object for invokeCommand.
 * @param method_id MINK_OP_OPEN_SESSION or MINK_OP_INVOKE_COMMAND.
 * @param n Number of slots the method uses, at most MINK_WIRE_MAX_SLOTS.
 * @param bi_uuid Slot of the destination UUID, or -1.
 * @param uuid The destination UUID.
 * @param uuid_len Size of @p uuid.
 * @param bi_scalars Slot of the packed input scalars.
 * @param in_scalars The packed input scalars, struct os_in or struct ic_in.
 * @param in_len Size of @p in_scalars.
 * @param bi_param0 Slot of the first input memref buffer.
 * @param bo_scalars Slot of the packed output scalars.
 * @param out Receives the packed output scalars.
 * @param bo_param0 Slot of the first output memref buffer.
 * @param oi_waiter Slot of the cancellation waiter, or -1.
 * @param waiter_obj The cancellation waiter, borrowed.
 * @param oi_param0 Slot of the first memref memory object.
 * @param call_param The translated parameters.
 * @param oo_session Slot of the session object, or -1.
 * @param session_obj_out Receives the session object, owned by the caller.
 * @param result Receives the result QTEE returned for the method itself.
 * @return 0 if the request reached QTEE and @p result is meaningful.
 *         -1 if it did not, in which case @p result is untouched.
 */
static int mink_wire_invoke(teec_obj_t target, uint32_t method_id, int n,
			  int bi_uuid, const void *uuid, size_t uuid_len,
			  int bi_scalars, const void *in_scalars, size_t in_len,
			  int bi_param0, int bo_scalars,
			  struct mink_out_scalars *out, int bo_param0,
			  int oi_waiter, teec_obj_t waiter_obj, int oi_param0,
			  const MINK_Parameter *call_param, int oo_session,
			  teec_obj_t *session_obj_out, qcomtee_result_t *result)
{
	struct qcomtee_param p[MINK_WIRE_MAX_SLOTS] = { 0 };

	if (bi_uuid >= 0)
		UBUF_IN(p[bi_uuid], uuid, uuid_len);
	UBUF_IN(p[bi_scalars], in_scalars, in_len);
	UBUF_OUT(p[bo_scalars], out, sizeof(*out));
	if (oi_waiter >= 0)
		oi_fill(&p[oi_waiter], waiter_obj);
	if (oo_session >= 0)
		OBJ_OUT(p[oo_session]);

	for (int i = 0; i < MAX_NUM_PARAMS; i++) {
		UBUF_IN(p[bi_param0 + i], call_param[i].in_buf.buf,
			call_param[i].in_buf.len);
		UBUF_OUT(p[bo_param0 + i], call_param[i].out_buf.buf,
			 call_param[i].out_buf.len);
		oi_fill(&p[oi_param0 + i], call_param[i].mem_obj);
	}

	if (qcomtee_object_invoke(target, method_id, p, n, result)) {
		/* The waiter, when present, sits directly ahead of the memory
		 * object slots, so one sweep covers every slot oi_fill()
		 * touched.
		 */
		int oi_first = (oi_waiter >= 0) ? oi_waiter : oi_param0;

		oi_rollback(p, oi_first,
			    oi_param0 + MAX_NUM_PARAMS - oi_first);

		return -1;
	}

	if (*result)
		return 0;

	/* Plain assignment: QTEE hands over its reference, so retaining here
	 * would leak. Note that this happens even when the GP level
	 * out->ret_value is an error, because the method itself succeeded;
	 * disposing of the object in that case is up to the caller.
	 */
	if (oo_session >= 0)
		*session_obj_out = p[oo_session].object;

	/* QTEE reports the size it actually produced in the packed scalars.
	 * A zero there means it left the buffer alone, in which case the size
	 * the slot still carries is what the caller asked for.
	 */
	for (int i = 0; i < MAX_NUM_PARAMS; i++) {
		if (!call_param[i].out_buf.len_out)
			continue;

		*call_param[i].out_buf.len_out =
			out->memref_sz[i] ? out->memref_sz[i]
					  : p[bo_param0 + i].ubuf.size;
	}

	return 0;
}

TEEC_Result initialize_context(TEEC_Context *ctx)
{
	struct supplicant *sup = NULL;
	struct qcomtee_object *creds = TEEC_OBJ_NULL;
	struct qcomtee_object *client_env = TEEC_OBJ_NULL;
	struct qcomtee_param p[2] = { 0 };
	qcomtee_result_t result = QCOMTEE_OK;
	uint32_t uid = MINK_CGPAPPCLIENT_UID;

	if (!ctx)
		return TEEC_ERROR_BAD_PARAMETERS;

	/* Nothing below assumes the caller handed us a cleared struct, and the
	 * error paths release whatever is set, so start from a known state.
	 */
	ctx->imp.root_obj = TEEC_OBJ_NULL;
	ctx->imp.app_client = TEEC_OBJ_NULL;
	ctx->imp.waiter_cbo = TEEC_OBJ_NULL;

	/* Opens /dev/tee0 and starts the threads that serve QTEE's callback
	 * requests. The root object owns the supplicant: releasing the last
	 * reference to it is what stops those threads again.
	 */
	sup = supplicant_start(DEFAULT_CBOBJ_THREAD_CNT);
	if (!sup)
		return TEEC_ERROR_GENERIC;
	ctx->imp.root_obj = sup->root;

	if (qcomtee_object_credentials_init(ctx->imp.root_obj, &creds))
		goto err_root;

	/* The credentials object is handed over rather than lent, so the slot
	 * is filled directly instead of through oi_fill(): our single
	 * reference becomes QTEE's. Only a transport failure leaves it with
	 * us, and only then do we release it.
	 */
	p[0].attr = QCOMTEE_OBJREF_INPUT;
	p[0].object = creds;
	OBJ_OUT(p[1]);

	if (qcomtee_object_invoke(ctx->imp.root_obj, MINK_OP_REGISTER_AS_CLIENT,
				  p, 2, &result)) {
		qcomtee_object_refs_dec(creds);
		goto err_root;
	}
	if (result)
		goto err_root;

	client_env = p[1].object;

	memset(p, 0, sizeof(p));
	UBUF_IN(p[0], &uid, sizeof(uid));
	OBJ_OUT(p[1]);

	if (qcomtee_object_invoke(client_env, MINK_OP_CLIENT_ENV_OPEN, p, 2,
				  &result))
		goto err_client_env;
	if (result)
		goto err_client_env;

	ctx->imp.app_client = p[1].object;

	/* The waiter has to share the session's root, or QTEE would refuse it
	 * as belonging to a different namespace.
	 */
	if (cwait_open(ctx->imp.root_obj, &ctx->imp.waiter_cbo))
		goto err_app_client;

	/* The client environment was only needed to reach the app client. */
	qcomtee_object_refs_dec(client_env);

	return TEEC_SUCCESS;

err_app_client:
	TEEC_OBJ_RELEASE(ctx->imp.app_client);
err_client_env:
	qcomtee_object_refs_dec(client_env);
err_root:
	TEEC_OBJ_RELEASE(ctx->imp.root_obj);

	return TEEC_ERROR_GENERIC;
}

void finalize_context(TEEC_Context *ctx)
{
	if (!ctx)
		return;

	/* The waiter holds a reference on the root and the app client is a
	 * QTEE object reached through it, so the root goes last: dropping its
	 * last reference is what tears down the supplicant threads, and they
	 * must still be running while anything else is being released.
	 */
	TEEC_OBJ_RELEASE(ctx->imp.waiter_cbo);
	TEEC_OBJ_RELEASE(ctx->imp.app_client);
	TEEC_OBJ_RELEASE(ctx->imp.root_obj);
}

/**
 * @brief Turn a QTEE level error into the GP result/origin pair.
 *
 * Reproduces the table mink_teec.c applies to the return value of the generated
 * stub, so both backends report the same thing for the same failure.
 *
 * @param rv The error QTEE reported, never QCOMTEE_OK.
 * @param result Receives the GP result code.
 * @param eorigin Receives the GP origin.
 */
static void map_err(qcomtee_result_t rv, TEEC_Result *result, uint32_t *eorigin)
{
	/* qcomtee_result_t is unsigned, but the error space it carries is
	 * signed: every QCOMTEE_ERROR_* below is negative. Without the
	 * reinterpretation each of them would compare unequal and fall through
	 * to the generic case. libminkadaptor relies on the same
	 * reinterpretation, by assigning the result to an int32_t.
	 */
	int32_t e = (int32_t)rv;

	if (e == QCOMTEE_ERROR_DEFUNCT) {
		*result = TEEC_ERROR_TARGET_DEAD;
		*eorigin = TEEC_ORIGIN_TEE;
	} else if (e == QCOMTEE_ERROR_BUSY) {
		*result = TEEC_ERROR_BUSY;
		*eorigin = TEEC_ORIGIN_TEE;
	} else if (e == QCOMTEE_ERROR_KMEM || e == QCOMTEE_ERROR_NOSLOTS) {
		*result = TEEC_ERROR_OUT_OF_MEMORY;
		*eorigin = TEEC_ORIGIN_TEE;
	} else {
		*result = TEEC_ERROR_GENERIC;
		*eorigin = TEEC_ORIGIN_COMMS;
	}

	/* Note that no branch yields TEEC_ORIGIN_TRUSTED_APP, which is what
	 * open_session() keys the disposal of the session object off. A
	 * transport or QTEE level failure therefore never leaves an object
	 * behind to dispose of, and indeed mink_wire_invoke() did not write one.
	 */
}

/**
 * @brief Issue an IGPAppClient.openSession request.
 *
 * @param app_client The application client obtained at context initialization.
 * @param waiter_cbo The cancellation waiter, borrowed.
 * @param destination UUID of the trusted application to reach.
 * @param conn_method The GP connection method.
 * @param conn_data The GP connection data.
 * @param call The translated parameters and the three type/code words.
 * @param session_obj Receives the session object, owned by the caller.
 * @param eorigin Receives the GP origin.
 * @return The GP result code: what QTEE returned for the method when it ran it,
 *         otherwise the mapping of the failure that kept it from running.
 */
static TEEC_Result mink_open_session_invoke(teec_obj_t app_client,
					  teec_obj_t waiter_cbo,
					  const TEEC_UUID *destination,
					  uint32_t conn_method,
					  uint32_t conn_data,
					  const struct mink_call *call,
					  teec_obj_t *session_obj,
					  uint32_t *eorigin)
{
	struct os_in in = {
		.cancel_code = call->cancel_code,
		.conn_method = conn_method,
		.conn_data = conn_data,
		.param_types = call->param_types,
		.ex_param_types = call->ex_param_types,
	};
	struct mink_out_scalars out = { 0 };
	qcomtee_result_t rv = QCOMTEE_OK;
	TEEC_Result result = TEEC_SUCCESS;

	if (mink_wire_invoke(app_client, MINK_OP_OPEN_SESSION, OS_SLOT_COUNT,
			   OS_BI_UUID, destination, sizeof(*destination),
			   OS_BI_SCALARS, &in, sizeof(in), OS_BI_PARAM0,
			   OS_BO_SCALARS, &out, OS_BO_PARAM0, OS_OI_WAITER,
			   waiter_cbo, OS_OI_MEM0, call->param, OS_OO_SESSION,
			   session_obj, &rv)) {
		/* The request never reached QTEE, so there is no specific code
		 * to report. libminkadaptor flattens this case to Object_ERROR
		 * before mink_teec.c gets to look at it, which lands in the
		 * generic branch of the very same table.
		 */
		map_err(QCOMTEE_ERROR, &result, eorigin);

		return result;
	}

	if (rv) {
		map_err(rv, &result, eorigin);

		return result;
	}

	*eorigin = out.ret_origin;

	return out.ret_value;
}

TEEC_Result open_session(TEEC_Context *ctx, TEEC_Session *session,
			 const TEEC_UUID *destination, uint32_t conn_method,
			 const void *connection_data, TEEC_Operation *op,
			 uint32_t *ret_origin)
{
	struct mink_call call = { 0 };
	TEEC_Result result = TEEC_SUCCESS;
	uint32_t eorigin = TEEC_ORIGIN_COMMS;
	uint32_t conn_data = 0;

	if (!ctx || !session || !destination)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (connection_data)
		conn_data = *(const uint32_t *)connection_data;

	if (ret_origin)
		*ret_origin = TEEC_ORIGIN_COMMS;

	/* Zeroing struct mink_call is not enough: the memref size write-back
	 * needs out_buf.len_out to point back at out_buf.len.
	 */
	mink_params_INIT(call.param);

	if (op) {
		call.cancel_code = (rand() & CANCEL_CODE_MASK);
		op->imp.cancel_code = call.cancel_code;
		op->imp.session = session;

		result = memref_temp_to_partial_params(ctx, &(op->paramTypes),
						       op->params);
		if (result)
			return result;

		mink_params_from_teec_params(op->paramTypes, op->params,
					   call.param, &call.ex_param_types);
		tee_types_from_teec_types(op, &call.param_types);
	}

	result = mink_open_session_invoke(ctx->imp.app_client, ctx->imp.waiter_cbo,
					destination, conn_method, conn_data,
					&call, &(session->imp.session_obj),
					&eorigin);
	if (result)
		MSGE("mink_open_session_invoke() failed: 0x%x\n", result);

	if (result) {
		/* QTEE hands back a session object even when the trusted
		 * application is the one that refused the session, and the
		 * caller must not be left holding it.
		 */
		if (eorigin == TEEC_ORIGIN_TRUSTED_APP)
			TEEC_OBJ_RELEASE(session->imp.session_obj);
	} else {
		session->imp.ctx = ctx;
	}

	if (ret_origin)
		*ret_origin = eorigin;

	if (op) {
		update_shm_memref_from_mem_obj(op->paramTypes, op->params);
		memref_temp_from_partial_params(&(op->paramTypes), op->params);
	}

	return result;
}

/**
 * @brief Issue an IGPSession.invokeCommand request.
 *
 * Three of mink_wire_invoke()'s slots do not exist here: the destination UUID,
 * the cancellation waiter and the output session object, all of which are
 * openSession specific and are therefore switched off with -1.
 *
 * @param session_obj The session to invoke the command on.
 * @param command_id Identifier of the command to invoke.
 * @param call The translated parameters and the three type/code words.
 * @param eorigin Receives the GP origin.
 * @return The GP result code: what QTEE returned for the method when it ran it,
 *         otherwise the mapping of the failure that kept it from running.
 */
static TEEC_Result mink_invoke_command_invoke(teec_obj_t session_obj,
					    uint32_t command_id,
					    const struct mink_call *call,
					    uint32_t *eorigin)
{
	struct ic_in in = {
		.cmd_id = command_id,
		.cancel_code = call->cancel_code,
		/* Not exposed to GP callers today, so the wire value is the
		 * constant the existing backend passes rather than a new
		 * configuration knob.
		 */
		.timeout = MINK_TEEC_TIMEOUT_INFINITE,
		.param_types = call->param_types,
		.ex_param_types = call->ex_param_types,
	};
	struct mink_out_scalars out = { 0 };
	qcomtee_result_t rv = QCOMTEE_OK;
	TEEC_Result result = TEEC_SUCCESS;

	if (mink_wire_invoke(session_obj, MINK_OP_INVOKE_COMMAND, IC_SLOT_COUNT, -1,
			   NULL, 0, IC_BI_SCALARS, &in, sizeof(in),
			   IC_BI_PARAM0, IC_BO_SCALARS, &out, IC_BO_PARAM0, -1,
			   TEEC_OBJ_NULL, IC_OI_MEM0, call->param, -1, NULL,
			   &rv)) {
		map_err(QCOMTEE_ERROR, &result, eorigin);
		/* Bit-faithful to mink_invoke_command(), which follows the very
		 * same table with an unconditional override of the origin and
		 * so reports TEEC_ORIGIN_TEE even for the generic branch that
		 * just set TEEC_ORIGIN_COMMS. mink_open_session() has no such
		 * override. Replicated rather than corrected, to keep xtest
		 * results identical; whether it is a typo is tracked
		 * separately.
		 */
		*eorigin = TEEC_ORIGIN_TEE;

		return result;
	}

	if (rv) {
		map_err(rv, &result, eorigin);
		*eorigin = TEEC_ORIGIN_TEE;

		return result;
	}

	*eorigin = out.ret_origin;

	return out.ret_value;
}

TEEC_Result invoke_command(TEEC_Session *session, uint32_t command_id,
			   TEEC_Operation *op, uint32_t *ret_origin)
{
	struct mink_call call = { 0 };
	TEEC_Result result = TEEC_SUCCESS;
	uint32_t eorigin = TEEC_ORIGIN_COMMS;

	/* The context is only reached through the session, so a session that
	 * never came out of a successful open_session() is caught here rather
	 * than inside memref_temp_to_partial_params().
	 */
	if (!session || !session->imp.ctx)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (ret_origin)
		*ret_origin = TEEC_ORIGIN_COMMS;

	mink_params_INIT(call.param);

	if (op) {
		call.cancel_code = (rand() & CANCEL_CODE_MASK);
		op->imp.cancel_code = call.cancel_code;
		op->imp.session = session;

		/* The result is dropped on purpose: this is the one place the
		 * existing backend differs from open_session(), which does
		 * return early on a conversion failure. Replicated as is.
		 */
		memref_temp_to_partial_params(session->imp.ctx,
					      &(op->paramTypes), op->params);

		mink_params_from_teec_params(op->paramTypes, op->params,
					   call.param, &call.ex_param_types);
		tee_types_from_teec_types(op, &call.param_types);
	}

	result = mink_invoke_command_invoke(session->imp.session_obj, command_id,
					  &call, &eorigin);
	if (result)
		MSGE("mink_invoke_command_invoke() failed: 0x%x\n", result);

	if (ret_origin)
		*ret_origin = eorigin;

	if (op) {
		update_shm_memref_from_mem_obj(op->paramTypes, op->params);
		memref_temp_from_partial_params(&(op->paramTypes), op->params);
	}

	return result;
}

/**
 * @brief Create a memory object to back a shared memory block.
 *
 * Replaces MinkCom_getMemoryObject, whose two object conversions are gone here:
 * ctx->imp.root_obj already is the type libqcomtee wants, and so is the object
 * it produces.
 *
 * @param ctx The context whose root the object is to belong to.
 * @param size Size of the memory to back the object with.
 * @param out Receives the memory object, owned by the caller.
 * @return TEEC_SUCCESS on success, TEEC_ERROR_* on failure.
 */
static TEEC_Result mink_obj_mem_alloc(TEEC_Context *ctx, size_t size,
				    teec_obj_t *out)
{
	struct qcomtee_object *mo = TEEC_OBJ_NULL;

	/* Stands in for the !root check MinkCom_getMemoryObject performs. */
	if (!ctx || TEEC_OBJ_IS_NULL(ctx->imp.root_obj))
		return TEEC_ERROR_BAD_PARAMETERS;

	/* TEEC_ERROR_GENERIC rather than TEEC_ERROR_OUT_OF_MEMORY: the
	 * allocation failure is only one of the reasons libqcomtee reports
	 * here, and both call sites used to flatten this path to GENERIC.
	 */
	if (qcomtee_memory_object_alloc(size, ctx->imp.root_obj, &mo))
		return TEEC_ERROR_GENERIC;

	*out = mo;

	return TEEC_SUCCESS;
}

int mink_obj_mem_info(teec_obj_t mem_obj, void **addr, size_t *size)
{
	/* The type check keeps a non-memory object from being read as one; it
	 * is an explicit defence of the existing implementation, not an
	 * accident, so it is kept. qcomtee_object_typeof() is NULL safe, but
	 * the explicit test documents that a NULL handle is expected here:
	 * every shared memory at or below TEEC_SHM_MAX_HEAP_SZ has one.
	 */
	if (TEEC_OBJ_IS_NULL(mem_obj) ||
	    qcomtee_object_typeof(mem_obj) != QCOMTEE_OBJECT_TYPE_MEMORY)
		return -1;

	*addr = qcomtee_memory_object_addr(mem_obj);
	*size = qcomtee_memory_object_size(mem_obj);

	return (*addr && *size) ? 0 : -1;
}

TEEC_Result register_shared_memory(TEEC_Context *ctx, TEEC_SharedMemory *shm,
				   uint8_t convert)
{
	teec_obj_t mo = TEEC_OBJ_NULL;
	TEEC_Result result = TEEC_SUCCESS;

	if (!ctx || !shm)
		return TEEC_ERROR_BAD_PARAMETERS;

	/* Only large buffers are backed by a memory object; anything at or
	 * below the threshold travels as a plain input/output buffer and
	 * occupies no object slot. The threshold is local policy, not wire
	 * ABI, and is left where it is.
	 */
	if (shm->size > TEEC_SHM_MAX_HEAP_SZ) {
		result = mink_obj_mem_alloc(ctx, shm->size, &mo);
		if (result)
			return result;
	}

	/* shm->buffer is deliberately untouched: a registered memory's buffer
	 * belongs to the client application.
	 */
	shm->imp.type = TEEC_MEMORY_REGISTERED;
	shm->imp.converted = convert;
	shm->imp.mem_obj = mo;
	shm->imp.ctx = ctx;

	return TEEC_SUCCESS;
}

TEEC_Result allocate_shared_memory(TEEC_Context *ctx, TEEC_SharedMemory *shm)
{
	teec_obj_t mo = TEEC_OBJ_NULL;
	TEEC_Result result = TEEC_SUCCESS;
	/* The object is page aligned, so this comes back larger than the
	 * requested size. It is a local upper bound only: shm->size keeps the
	 * value the caller asked for, as GP requires.
	 */
	size_t mo_size = 0;

	if (!ctx || !shm)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (shm->size > TEEC_SHM_MAX_HEAP_SZ) {
		result = mink_obj_mem_alloc(ctx, shm->size, &mo);
		if (result)
			return result;

		if (mink_obj_mem_info(mo, &shm->buffer, &mo_size)) {
			/* Without this the object and its mapping both leak. */
			qcomtee_memory_object_release(mo);

			return TEEC_ERROR_GENERIC;
		}
	} else {
		shm->buffer = malloc(shm->size);
		if (!shm->buffer)
			return TEEC_ERROR_OUT_OF_MEMORY;
	}

	/* Note that imp.converted is left alone: register_shared_memory() is
	 * the only entry point that sets it.
	 */
	shm->imp.type = TEEC_MEMORY_ALLOCATED;
	shm->imp.mem_obj = mo;
	shm->imp.ctx = ctx;

	return TEEC_SUCCESS;
}

void release_shared_memory(TEEC_SharedMemory *shm)
{
	if (!shm)
		return;

	/* The order of the two steps below is load bearing. For an allocated
	 * block above the threshold shm->buffer points into the memory
	 * object's mapping, which is unmapped the moment the last reference
	 * goes; clearing the fields first is what keeps a dangling pointer
	 * from being left behind.
	 *
	 * For the same reason the size test has to happen before shm->size is
	 * cleared: reversing the two would send every large buffer into
	 * free(), which is undefined on a mapping.
	 */
	if (shm->imp.type == TEEC_MEMORY_ALLOCATED) {
		if (shm->size <= TEEC_SHM_MAX_HEAP_SZ)
			free(shm->buffer);

		shm->buffer = NULL;
		shm->size = 0;
	}

	/* A no-op when the block had no backing object. */
	TEEC_OBJ_RELEASE(shm->imp.mem_obj);

	shm->imp.converted = 0;
	shm->imp.type = TEEC_MEMORY_FREE;
	shm->imp.ctx = NULL;
}

void close_session(TEEC_Session *session)
{
	if (!session)
		return;

	/* IGPSession.close is deliberately not invoked. Tearing the session
	 * down is driven purely by the session object's reference count
	 * dropping to zero, which QTEE observes on its own; sending an explicit
	 * close on top of that would deliver a second teardown to the trusted
	 * application, a behaviour change rather than an optimization.
	 *
	 * The release has to tolerate a NULL handle: open_session() nulls the
	 * session object when the trusted application refuses the session, and
	 * the caller may still close afterwards.
	 */
	TEEC_OBJ_RELEASE(session->imp.session_obj);
	session->imp.ctx = NULL;
}

void request_cancellation(TEEC_Operation *op)
{
	TEEC_Session *session = NULL;
	TEEC_Context *ctx = NULL;
	teec_obj_t waiter_cbo = TEEC_OBJ_NULL;

	/* The waiter lives on the context, which is only reachable through the
	 * session, so both links are checked before either is followed.
	 */
	if (!op || !op->imp.session)
		return;

	session = op->imp.session;
	ctx = session->imp.ctx;
	if (!ctx)
		return;

	waiter_cbo = ctx->imp.waiter_cbo;

	if (TEEC_OBJ_IS_NULL(waiter_cbo)) {
		MSGE("Waiter CBO not available!\n");
		return;
	}

	/* The waiter is this process's own object, so the signal is a direct
	 * call: the three layers the old path went through, IWait_signal
	 * packing, Object_invoke dispatching and IWait_invoke unpacking, all
	 * disappear.
	 *
	 * The cancel code goes out raw. CANCEL_CODE_MASK is applied where the
	 * code is generated, not where it is delivered; masking again would be
	 * a no-op today but would silently stop matching QTEE's code should the
	 * generated range ever widen, and cancellation would just quietly fail.
	 */
	cwait_signal(CWAIT_OF(waiter_cbo), op->imp.cancel_code,
		     MINK_EVENT_CANCEL);
}
