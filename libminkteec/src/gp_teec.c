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
	uint32_t uid = GP_CGPAPPCLIENT_UID;

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

	if (qcomtee_object_invoke(ctx->imp.root_obj, GP_OP_REGISTER_AS_CLIENT,
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

	if (qcomtee_object_invoke(client_env, GP_OP_CLIENT_ENV_OPEN, p, 2,
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
