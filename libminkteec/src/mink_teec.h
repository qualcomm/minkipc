// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __MINK_TEEC_H_
#define __MINK_TEEC_H_

/* Private header of the libminkadaptor backend.
 *
 * Declares the nine entry points the public API calls down into, implemented
 * here by mink_teec.c on top of the idlc generated stubs. The direct
 * libqcomtee backend declares the very same nine in gp_teec.h, and
 * tee_client_api.c includes whichever of the two headers matches
 * MINKTEEC_DIRECT_QCOMTEE.
 *
 * Everything the two backends share - the GP parameter types, the TEEC_OBJ_*
 * handle helpers and the translation layer itself - lives in mink_params.h.
 */
#include "mink_params.h"

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
 * @param connection_method The method of connection to use.
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

#endif // __MINK_TEEC_H_
