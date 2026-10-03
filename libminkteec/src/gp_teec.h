// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __GP_TEEC_H_
#define __GP_TEEC_H_

/* Private header of the direct libqcomtee backend.
 *
 * Two things live here. First, the nine entry points the public API calls down
 * into, declared at the bottom of the file and implemented by gp_teec.c.
 * Second, and above them, the wire ABI: this header is the single place where
 * libminkteec knows what QTEE's GP interfaces look like on the wire - method
 * IDs, the slot each parameter occupies in the qcomtee_param array, and the
 * layout of the two packed scalar blocks. gp_teec.c marshals against these
 * declarations and nothing else; the translation layer in mink_params.c stays
 * wire agnostic.
 *
 * Only built when MINKTEEC_DIRECT_QCOMTEE is on. The libminkadaptor backend
 * (mink_teec.c) reaches the same methods through the idlc generated stubs and
 * declares its own copy of the nine entry points in mink_teec.h, so it never
 * includes this file.
 */

#include <stddef.h>
#include <stdint.h>

#include "tee_client_api.h"

/* mink_params.h brings in qcomtee_object.h, which defines container_of without
 * a guard, and has to stay ahead of gp_wait.h, whose qlist.h guards the very
 * same macro. gp_wait.h in turn pulls in <pthread.h> for supplicant.h.
 */
#include "mink_params.h"
#include "qcomtee_object_types.h"
#include "gp_wait.h"
#include "transport/supplicant.h"

/**
 * @brief State for a direct-backend GP request.
 *
 * Everything an IGPAppClient.openSession or IGPSession.invokeCommand request
 * carries besides the method's own scalars: the MINK-translated parameters and
 * the three type/code words derived from the TEEC_Operation.
 *
 * Zero-initializing this struct is not sufficient: after zero initialization,
 * mink_params_INIT(call.param) must run because out_buf.len_out has to point
 * back at out_buf.len for memref-size write-back to land anywhere.
 */
struct mink_call {
	MINK_Parameter param[MAX_NUM_PARAMS];
	uint32_t param_types;
	uint32_t ex_param_types;
	uint32_t cancel_code;
};

/* Number of supplicant threads serving QTEE's callback requests. Mirrors
 * DEFAULT_CBOBJ_THREAD_CNT in libminkadaptor/src/mink_adaptor_priv.h:24, which
 * is private to that library and hence restated rather than included.
 */
#define DEFAULT_CBOBJ_THREAD_CNT 4

/* Method IDs, in the declaration order of the respective .idl file.
 *
 * IClientEnv.open / .registerLegacy / .registerAsClient are declared in
 * idl/IClientEnv.idl in that order, so registerAsClient is 2. IGPAppClient
 * declares openSession before openSessionV2, IGPSession close before
 * invokeCommand.
 */
#define MINK_OP_CLIENT_ENV_OPEN    0 /* IClientEnv.open            */
#define MINK_OP_REGISTER_AS_CLIENT 2 /* IClientEnv.registerAsClient */
#define MINK_OP_OPEN_SESSION       0 /* IGPAppClient.openSession    */
#define MINK_OP_CLOSE              0 /* IGPSession.close            */
#define MINK_OP_INVOKE_COMMAND     1 /* IGPSession.invokeCommand    */

/* UID of the GP application client service, from idl/CGPAppClient.idl. */
#define MINK_CGPAPPCLIENT_UID 0x199

/* Slot layout.
 *
 * A qcomtee_param array is grouped by class, in the order
 *
 *     input buffers -> output buffers -> input objects -> output objects
 *
 * with declaration order preserved inside each group. On top of that, idlc
 * bundles the scalar parameters of a method into one buffer per direction as
 * soon as there is more than one of them, and places that bundle at the head
 * of its group: the input bundle is emitted before every other parameter, the
 * output bundle immediately before the first output buffer.
 *
 * Both rules are visible in the generated stubs checked into this repository,
 * for instance mink_platform/mink_test/minktransport_test/service/inc/
 * ITestUtils.h (bufferEcho for the group order, invocationTransfer for all
 * four groups, bufferPlus for a bundled input) and .../ITestModule.h
 * (setMultiId, getMultiId for the bundle contents).
 *
 * Hence for IGPAppClient.openSession, whose formals are
 *
 *     in buffer uuid, in interface waitCBO,
 *     in uint32 cancelCode, connectionMethod, connectionData,
 *               paramTypes, exParamTypes,
 *     in buffer i1..i4, out buffer o1..o4, in interface imem1..imem4,
 *     out uint32 memrefOutSz1..memrefOutSz4,
 *     out interface session, out uint32 retValue, retOrigin
 *
 * the seventeen slots come out as below. Note that the five input scalars
 * precede the uuid buffer even though the IDL declares uuid first.
 */
enum mink_os_slot {
	OS_BI_SCALARS = 0,                        /* struct os_in         */
	OS_BI_UUID,                               /* TEEC_UUID            */
	OS_BI_PARAM0,                             /* i1..i4               */
	OS_BO_SCALARS = OS_BI_PARAM0 + MAX_NUM_PARAMS, /* struct mink_out_scalars */
	OS_BO_PARAM0,                             /* o1..o4               */
	OS_OI_WAITER = OS_BO_PARAM0 + MAX_NUM_PARAMS, /* waitCBO           */
	OS_OI_MEM0,                               /* imem1..imem4         */
	OS_OO_SESSION = OS_OI_MEM0 + MAX_NUM_PARAMS, /* session            */
	OS_SLOT_COUNT
};

_Static_assert(OS_BI_SCALARS == 0 && OS_BI_UUID == 1 && OS_BI_PARAM0 == 2 &&
		       OS_BO_SCALARS == 6 && OS_BO_PARAM0 == 7 &&
		       OS_OI_WAITER == 11 && OS_OI_MEM0 == 12 &&
		       OS_OO_SESSION == 16 && OS_SLOT_COUNT == 17,
	       "IGPAppClient.openSession slot layout drift");

/* IGPSession.invokeCommand, whose formals are
 *
 *     in uint32 commandID, cancelCode, cancellationRequestTimeout,
 *               paramTypes, exParamTypes,
 *     in buffer i1..i4, out buffer o1..o4, in interface imem1..imem4,
 *     out uint32 memrefOutSz1..memrefOutSz4, retValue, retOrigin
 *
 * has no uuid, no waiter and no output object, leaving fourteen slots. Its
 * first formal is already a scalar, so the input bundle changes nothing here.
 */
enum mink_ic_slot {
	IC_BI_SCALARS = 0,                        /* struct ic_in         */
	IC_BI_PARAM0,                             /* i1..i4               */
	IC_BO_SCALARS = IC_BI_PARAM0 + MAX_NUM_PARAMS, /* struct mink_out_scalars */
	IC_BO_PARAM0,                             /* o1..o4               */
	IC_OI_MEM0 = IC_BO_PARAM0 + MAX_NUM_PARAMS, /* imem1..imem4       */
	IC_SLOT_COUNT = IC_OI_MEM0 + MAX_NUM_PARAMS
};

_Static_assert(IC_BI_SCALARS == 0 && IC_BI_PARAM0 == 1 && IC_BO_SCALARS == 5 &&
		       IC_BO_PARAM0 == 6 && IC_OI_MEM0 == 10 &&
		       IC_SLOT_COUNT == 14,
	       "IGPSession.invokeCommand slot layout drift");

/* Size of the slot array mink_wire_invoke() puts on the stack. */
#define MINK_WIRE_MAX_SLOTS 17

_Static_assert(MINK_WIRE_MAX_SLOTS >= OS_SLOT_COUNT &&
		       MINK_WIRE_MAX_SLOTS >= IC_SLOT_COUNT,
	       "MINK_WIRE_MAX_SLOTS too small for the widest method");

/* Packed scalar blocks.
 *
 * idlc sorts the members of a bundle by descending type size and keeps the
 * declaration order among members of equal size. Every scalar of the two GP
 * methods is a uint32, so the sort degenerates to declaration order and the
 * structs below need no reordering. A future interface mixing uint64 and
 * uint32 scalars would not be that lucky.
 */
struct os_in {
	uint32_t cancel_code;
	uint32_t conn_method;
	uint32_t conn_data;
	uint32_t param_types;
	uint32_t ex_param_types;
};

_Static_assert(sizeof(struct os_in) == 20,
	       "openSession in-scalar pack drift");

struct ic_in {
	uint32_t cmd_id;
	uint32_t cancel_code;
	uint32_t timeout;
	uint32_t param_types;
	uint32_t ex_param_types;
};

_Static_assert(sizeof(struct ic_in) == 20,
	       "invokeCommand in-scalar pack drift");

/* Both methods declare the same output scalars in the same order, so one
 * struct serves both. openSession interleaves its output object between
 * memrefOutSz4 and retValue, but bundling only ever collects scalars.
 */
struct mink_out_scalars {
	uint32_t memref_sz[MAX_NUM_PARAMS];
	uint32_t ret_value;
	uint32_t ret_origin;
};

_Static_assert(sizeof(struct mink_out_scalars) == 24, "out-scalar pack drift");

/* Slot fill helpers.
 *
 * Every slot of the array has to be given an attribute, including the ones
 * whose payload is empty, because qcomtee_object_invoke() walks all num_params
 * entries. An unused buffer slot is a well formed zero length buffer.
 */
#define UBUF_IN(s, a, n)                                                       \
	do {                                                                   \
		(s).attr = QCOMTEE_UBUF_INPUT;                                 \
		(s).ubuf.addr = (void *)(a);                                   \
		(s).ubuf.size = (n);                                           \
	} while (0)

#define UBUF_OUT(s, a, n)                                                      \
	do {                                                                   \
		(s).attr = QCOMTEE_UBUF_OUTPUT;                                \
		(s).ubuf.addr = (void *)(a);                                   \
		(s).ubuf.size = (n);                                           \
	} while (0)

#define OBJ_OUT(s)                                                             \
	do {                                                                   \
		(s).attr = QCOMTEE_OBJREF_OUTPUT;                              \
		(s).object = TEEC_OBJ_NULL;                                    \
	} while (0)

/* Backend entry points.
 *
 * The nine functions the public API in tee_client_api.c calls down into,
 * implemented by gp_teec.c. mink_teec.h declares the same nine for the
 * libminkadaptor backend; the two contracts are identical, which is what lets
 * tee_client_api.c stay backend agnostic apart from picking a header.
 */

/**
 * @brief Initializes a new TEE Context, forming a connection between the
 * Client Application and QTEE.
 *
 * @param ctx The TEE context to be initialized.
 * @return TEEC_SUCCESS if the initialization was successful.
 *	   TEEC_ERROR_* otherwise.
 */
TEEC_Result initialize_context(TEEC_Context *ctx);

/**
 * @brief Finalizes an initialized TEE Context, closing the connection
 * between the Client Application and QTEE.
 *
 * @param ctx The TEE context to be finalized.
 */
void finalize_context(TEEC_Context *ctx);

/**
 * @brief Opens a new Session between the Client Application and the specified
 * Trusted Application in QTEE.
 *
 * @param ctx The initialized TEE context over which to establish a session.
 * @param session The session to be established with QTEE.
 * @param destination The UUID of the destination Trusted Application.
 * @param conn_method The method of connection to use.
 * @param connection_data Any necessary data required to support the connection
 *                        method chosen.
 * @param op The optional operation payload for this request.
 * @param ret_origin The origin of the returned value from QTEE.
 * @return TEEC_SUCCESS If the session was initialized successfully.
 *	   TEEC_ERROR_* otherwise.
 */
TEEC_Result open_session(TEEC_Context *ctx, TEEC_Session *session,
			 const TEEC_UUID *destination, uint32_t conn_method,
			 const void *connection_data, TEEC_Operation *op,
			 uint32_t *ret_origin);

/**
 * @brief Closes an established Session between the Client Application and a
 * Trusted Application in QTEE.
 *
 * @param session The session to be closed with QTEE.
 */
void close_session(TEEC_Session *session);

/**
 * @brief Invoke a command over an established Session to a Trusted Application
 * in QTEE.
 *
 * @param session The session over which to invoke the command.
 * @param command_id Identifier for the command to invoke.
 * @param op The optional operation payload for this request.
 * @param ret_origin The origin of the returned value from QTEE.
 * @return TEEC_SUCCESS If the command was invoked successfully.
 *	   TEEC_ERROR_* otherwise.
 */
TEEC_Result invoke_command(TEEC_Session *session, uint32_t command_id,
			   TEEC_Operation *op, uint32_t *ret_origin);

/**
 * @brief Register a shared memory with QTEE.
 *
 * @param ctx The initialized TEE context over which to register a shared
 *            memory.
 * @param shm The Shared Memory to be registered with QTEE.
 * @param convert Boolean indicating whether this shared memory was converted
 *                from a temporary memory.
 * @return TEEC_SUCCESS If the memory was registered successfully.
 *	   TEEC_ERROR_* otherwise.
 */
TEEC_Result register_shared_memory(TEEC_Context *ctx, TEEC_SharedMemory *shm,
				   uint8_t convert);

/**
 * @brief Allocate a memory shared with QTEE.
 *
 * @param ctx The initialized TEE context over which to allocate a shared
 *            memory.
 * @param shm The Shared Memory to be allocated.
 * @return TEEC_SUCCESS If the memory was allocated successfully.
 *	   TEEC_ERROR_* otherwise.
 */
TEEC_Result allocate_shared_memory(TEEC_Context *ctx, TEEC_SharedMemory *shm);

/**
 * @brief Release a memory shared with QTEE.
 *
 * @param shm The Shared Memory to be released.
 */
void release_shared_memory(TEEC_SharedMemory *shm);

/**
 * @brief Requests cancellation of a pending open Session operation or a
 * Command invocation operation.
 *
 * @param op The operation to be cancelled.
 */
void request_cancellation(TEEC_Operation *op);

#endif // __GP_TEEC_H_
