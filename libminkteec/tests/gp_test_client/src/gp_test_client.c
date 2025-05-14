// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "tee_client_api.h"
#include "gp_load_ta.h"

#define EXAMPLE_MULTIPLY_HLOS_BUFFER_CMD 1
#define GP_SAMPLE_WAIT_TEST 2
#define TEMP_MEM_CHECK_PARAMS 20
#define SHARED_MEM_CHECK_PARAMS 21

static struct option testopts[] = {
	{"preload", no_argument, NULL, 'l'},
	{"autoload", no_argument, NULL, 'a'},
	{NULL, 0, NULL, 0},
};

/* UUID for example_gpapp_ta_uuid */
const TEEC_UUID example_gpapp_ta_uuid = { 0x11111111,
					  0x1111,
					  0x1111,
					  { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
					    0x11, 0x11 } };

/* UUID for gptest app */
const TEEC_UUID gp_test_uuid = { 0xCAD10542,
				 0X34E4,
				 0X452D,
				 { 0X61, 0X56, 0XE9, 0X79, 0XAA, 0X6E, 0X61,
				   0XBC } };

/* UUID for gptest2 app */
const TEEC_UUID gp_test2_uuid = { 0x23914957,
				  0XC174,
				  0X4EA6,
				  { 0X54, 0XD1, 0XDA, 0X82, 0X0F, 0X77, 0XB4,
				    0XB0 } };

/* UUID for gpsample2 app */
const TEEC_UUID gp_sample2_uuid = { 0Xc02cac07,
				    0X2639,
				    0X4ef0,
				    { 0Xbc, 0x12, 0Xc4, 0Xaf, 0X1f, 0Xb3, 0Xe2,
				      0X76 } };

#define BUFFER_SIZE 0x1000
#define ALLOC_SIZE_02 0x2800
#define SIZE_02 0x2000
#define OFFSET_02 0x64

#define GP_TESTAPP_TEST_COUNTS 4

#define GP_HEAP_TESTS 11
#define GP_PROPERTY_TESTS 12
#define GP_TA_TA_TESTS 15
#define GP_TA_TA_NEG_TESTS 16

static TEEC_Result run_gptest_app_cmds(void)
{
	printf("==== [%s] START ====\n", __func__);

	TEEC_Context context = { 0 };
	TEEC_Session session = { 0 };
	TEEC_Operation operation = { 0 };
	uint32_t commands[] = {
		GP_HEAP_TESTS,
		GP_PROPERTY_TESTS,
		GP_TA_TA_TESTS,
		GP_TA_TA_NEG_TESTS,
	};

	TEEC_Result result = TEEC_ERROR_GENERIC;
	uint32_t return_origin = 0xFFFFFFFF;

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_InitializeContext failed, ret = 0x%x.\n", result);
		goto err_init_context;
	}

	result = TEEC_OpenSession(&context, &session, &gp_test_uuid,
				  TEEC_LOGIN_USER, NULL, NULL, &return_origin);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_OpenSession failed, ret = 0x%x.\n", result);
		goto err_open_sess;
	}

	/* Setup operation */
	operation.paramTypes =
		TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	for (size_t i = 0; i < GP_TESTAPP_TEST_COUNTS; i++) {
		result = TEEC_InvokeCommand(&session, commands[i], &operation,
					    &return_origin);

		if (commands[i] == GP_TA_TA_NEG_TESTS)
			result = result != TEEC_SUCCESS ? TEEC_SUCCESS :
							  TEEC_ERROR_GENERIC;

		if (result != TEEC_SUCCESS)
			printf("TEEC_InvokeCommand %d failed, ret = 0x%x.\n",
			       commands[i], result);
		else
			printf("TEEC_InvokeCommand %d passed.\n", commands[i]);
	}

	TEEC_CloseSession(&session);

err_open_sess:
	TEEC_FinalizeContext(&context);

err_init_context:

	printf("==== [%s] END ====\n", __func__);

	return result;
}

static TEEC_Result run_compare_tmp_buffer_test(void)
{
	printf("==== [%s] START ====\n", __func__);

	TEEC_Context context = { 0 };
	TEEC_Session session = { 0 };
	TEEC_Operation operation = { 0 };
	TEEC_TempMemoryReference tmpref = { 0 };
	TEEC_Result result = TEEC_ERROR_GENERIC;
	uint32_t return_origin = 0xFFFFFFFF;
	uint8_t check_buf[BUFFER_SIZE] = { 0 };

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_InitializeContext failed, ret = 0x%x.\n", result);
		goto err_init_context;
	}

	result = TEEC_OpenSession(&context, &session, &gp_test_uuid,
				  TEEC_LOGIN_USER, NULL, NULL, &return_origin);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_OpenSession failed, ret = 0x%x.\n", result);
		goto err_open_sess;
	}

	// Allocate temp mem
	tmpref.buffer = malloc(BUFFER_SIZE);
	if (!tmpref.buffer)
		goto err_tmp_malloc;
	tmpref.size = BUFFER_SIZE;

	/* Initialize buffer to all 1's */
	memset(tmpref.buffer, 0x1, tmpref.size);

	/* Initialize check_buf to all 1's */
	memset(check_buf, 0x1, sizeof(check_buf));

	/* Setup operation */
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
						TEEC_MEMREF_TEMP_INOUT,
						TEEC_NONE, TEEC_NONE);

	operation.params[0].value.a = 42;
	operation.params[1].tmpref = tmpref;

	result = TEEC_InvokeCommand(&session, EXAMPLE_MULTIPLY_HLOS_BUFFER_CMD,
				    &operation, &return_origin);

	if (result != TEEC_SUCCESS) {
		printf("TEEC_InvokeCommand failed, ret = 0x%x.\n", result);
		goto err_invoke_cmd;
	}

	/* Do the multiply locally */
	for (size_t cnt = 0; cnt < tmpref.size; ++cnt)
		*(check_buf + cnt) *= operation.params[0].value.a;

	if (memcmp(tmpref.buffer, check_buf, tmpref.size))
		printf("[TEST FAILED] Buffer comparison failed!\n");
	else
		printf("[TEST PASSED] Buffer comparison success.\n");

err_invoke_cmd:
	free(tmpref.buffer);

err_tmp_malloc:
	TEEC_CloseSession(&session);

err_open_sess:
	TEEC_FinalizeContext(&context);

err_init_context:

	printf("==== [%s] END ====\n", __func__);

	return result;
}

static TEEC_Result run_temp_memory_ref_test(void)
{
	printf("==== [%s] START ====\n", __func__);

	TEEC_Context context = { 0 };
	TEEC_Session session = { 0 };
	TEEC_Operation operation = { 0 };
	TEEC_TempMemoryReference tmpref1 = { 0 };
	TEEC_TempMemoryReference tmpref2 = { 0 };
	TEEC_Result result = TEEC_ERROR_GENERIC;
	uint32_t return_origin = 0xFFFFFFFF;
	uint8_t check_buf[BUFFER_SIZE] = { 0 };

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_InitializeContext failed, ret = 0x%x.\n", result);
		goto err_init_context;
	}

	result = TEEC_OpenSession(&context, &session, &gp_test_uuid,
				  TEEC_LOGIN_USER, NULL, NULL, &return_origin);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_OpenSession failed, ret = 0x%x.\n", result);
		goto err_open_sess;
	}

	// Initialize temp memory
	tmpref1.buffer = malloc(ALLOC_SIZE_02);
	tmpref1.size = ALLOC_SIZE_02;

	tmpref2.buffer = malloc(BUFFER_SIZE);
	tmpref2.size = BUFFER_SIZE;
	if (!tmpref1.buffer || !tmpref2.buffer)
		goto err_tmp_alloc;

	memset(tmpref1.buffer, 0x03, SIZE_02);
	memset((void *)(((uintptr_t)tmpref1.buffer)+SIZE_02), 0x04,
		ALLOC_SIZE_02-SIZE_02);
	memset(tmpref2.buffer, 0x1, BUFFER_SIZE);
	memset(check_buf, 0x1, BUFFER_SIZE);

	// Setup operation
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
						TEEC_MEMREF_TEMP_INOUT,
						TEEC_VALUE_INPUT, TEEC_NONE);

	operation.params[0].tmpref.buffer = tmpref1.buffer;
	operation.params[0].tmpref.size = SIZE_02;
	operation.params[1].tmpref.buffer = tmpref2.buffer;
	operation.params[1].tmpref.size = BUFFER_SIZE;
	operation.params[2].value.a = 72;

	result = TEEC_InvokeCommand(&session, TEMP_MEM_CHECK_PARAMS,
				    &operation, &return_origin);

	if (result != TEEC_SUCCESS) {
		printf("TEEC_InvokeCommand failed, ret = 0x%x.\n", result);
		goto err_invoke_cmd;
	}

	// Do the multiply locally
	for (size_t cnt = 0; cnt < tmpref2.size; ++cnt) {
		*(check_buf + cnt) *= operation.params[2].value.a;
	}

	if(memcmp(tmpref2.buffer, check_buf, tmpref2.size))
		printf("[TEST FAILED] Buffer comparison failed!\n");
	else
		printf("[TEST PASSED] Buffer comparison success.\n");

err_invoke_cmd:
	free(tmpref2.buffer);
	free(tmpref1.buffer);

err_tmp_alloc:
	TEEC_CloseSession(&session);

err_open_sess:
	TEEC_FinalizeContext(&context);

err_init_context:

	printf("==== [%s] END ====\n", __func__);

	return result;
}

static TEEC_Result run_allocate_partial_mem_offset_test(void)
{
	printf("==== [%s] START ====\n", __func__);

	TEEC_Context context = { 0 };
	TEEC_Session session = { 0 };
	TEEC_Operation operation = { 0 };
	TEEC_SharedMemory sharedMem = { 0 };
	TEEC_Result result = TEEC_ERROR_GENERIC;
	uint32_t return_origin = 0xFFFFFFFF;
	uint8_t check_buf[ALLOC_SIZE_02] = { 0 };

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_InitializeContext failed, ret = 0x%x.\n", result);
		goto err_init_context;
	}

	result = TEEC_OpenSession(&context, &session, &gp_test_uuid,
				  TEEC_LOGIN_USER, NULL, NULL, &return_origin);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_OpenSession failed, ret = 0x%x.\n", result);
		goto err_open_sess;
	}

	// Allocate shared memory
	sharedMem.size = ALLOC_SIZE_02;
	sharedMem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;

	printf("Allocate a shared buffer: size = 0%zX, flags = 0x%x.\n",
	       sharedMem.size, sharedMem.flags);
	result = TEEC_AllocateSharedMemory(&context, &sharedMem);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_AllocateSharedMemory failed, ret = 0x%x.\n",
		       result);
		goto err_alloc_shm;
	}

	// Initialize buffer to all 1's and 2's
	memset(sharedMem.buffer, 0x01, OFFSET_02);
	memset((void *)(((uintptr_t)sharedMem.buffer)+OFFSET_02), 0x02, SIZE_02);

	memset((void *)(((uintptr_t)sharedMem.buffer)+SIZE_02+OFFSET_02),
		0x03, sharedMem.size-SIZE_02-OFFSET_02);

	// Setup operation
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT,
						TEEC_MEMREF_PARTIAL_OUTPUT,
						TEEC_NONE, TEEC_NONE);
	operation.params[0].memref.parent = &sharedMem;
	operation.params[0].memref.offset = 0;
	operation.params[0].memref.size = OFFSET_02;
	operation.params[1].memref.parent = &sharedMem;
	operation.params[1].memref.offset = OFFSET_02;
	operation.params[1].memref.size = SIZE_02;

	result = TEEC_InvokeCommand(&session, SHARED_MEM_CHECK_PARAMS,
				    &operation, &return_origin);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_InvokeCommand failed, ret = 0x%x.\n", result);
		goto err_invoke_cmd;
	}

	// Fill check_buf with expected values
	memset(check_buf, 0x01, OFFSET_02);
	memset(check_buf+OFFSET_02, 0x04, SIZE_02);
	memset(check_buf+SIZE_02+OFFSET_02, 0x03,
	       ALLOC_SIZE_02-SIZE_02-OFFSET_02);

	if(memcmp(sharedMem.buffer, check_buf, ALLOC_SIZE_02))
		printf("[TEST FAILED] Buffer comparison failed!\n");
	else
		printf("[TEST PASSED] Buffer comparison success.\n");

err_invoke_cmd:
	TEEC_ReleaseSharedMemory(&sharedMem);

err_alloc_shm:
	TEEC_CloseSession(&session);

err_open_sess:
	TEEC_FinalizeContext(&context);

err_init_context:

	printf("==== [%s] END ====\n", __func__);

	return result;
}

static TEEC_Result run_register_partial_mem_offset_test(void)
{
	printf("==== [%s] START ====\n", __func__);

	TEEC_Context context = { 0 };
	TEEC_Session session = { 0 };
	TEEC_Operation operation = { 0 };
	TEEC_SharedMemory sharedMem = { 0 };
	TEEC_Result result = TEEC_ERROR_GENERIC;
	uint32_t return_origin = 0xFFFFFFFF;
	uint8_t check_buf[ALLOC_SIZE_02] = { 0 };

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_InitializeContext failed, ret = 0x%x.\n", result);
		goto err_init_context;
	}

	result = TEEC_OpenSession(&context, &session, &gp_test_uuid,
				  TEEC_LOGIN_USER, NULL, NULL, &return_origin);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_OpenSession failed, ret = 0x%x.\n", result);
		goto err_open_sess;
	}

	// Allocate shared mem
	sharedMem.buffer = malloc(ALLOC_SIZE_02);
	if (!sharedMem.buffer)
		goto err_shm_malloc;

	sharedMem.size = ALLOC_SIZE_02;
	sharedMem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;

	result = TEEC_RegisterSharedMemory(&context, &sharedMem);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_RegisterSharedMemory failed, ret = 0x%x.\n",
		       result);
		goto err_register_shm;
	}

	// Initialize buffer to all 1's and 2's
	memset(sharedMem.buffer, 0x01, OFFSET_02);
	memset((void *)(((uintptr_t)sharedMem.buffer)+OFFSET_02), 0x02,
		SIZE_02);
	memset((void *)(((uintptr_t)sharedMem.buffer)+SIZE_02+OFFSET_02),
		0x03, sharedMem.size-SIZE_02-OFFSET_02);

	// Setup operation
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_PARTIAL_INPUT,
						TEEC_MEMREF_PARTIAL_OUTPUT,
						TEEC_NONE, TEEC_NONE);
	operation.params[0].memref.parent = &sharedMem;
	operation.params[0].memref.offset = 0;
	operation.params[0].memref.size = OFFSET_02;
	operation.params[1].memref.parent = &sharedMem;
	operation.params[1].memref.offset = OFFSET_02;
	operation.params[1].memref.size = SIZE_02;

	result = TEEC_InvokeCommand(&session, SHARED_MEM_CHECK_PARAMS,
				    &operation, &return_origin);

	if (result != TEEC_SUCCESS) {
		printf("TEEC_InvokeCommand failed, ret = 0x%x.\n", result);
		goto err_invoke_cmd;
	}

	// Fill check_buf with expected values
	memset(check_buf, 0x01, OFFSET_02);
	memset(check_buf+OFFSET_02, 0x04, SIZE_02);
	memset(check_buf+SIZE_02+OFFSET_02, 0x03,
	       ALLOC_SIZE_02-SIZE_02-OFFSET_02);

	if (memcmp(sharedMem.buffer, check_buf, ALLOC_SIZE_02))
		printf("[TEST FAILED] Buffer comparison failed!\n");
	else
		printf("[TEST PASSED] Buffer comparison success.\n");

err_invoke_cmd:
	TEEC_ReleaseSharedMemory(&sharedMem);

err_register_shm:
	free(sharedMem.buffer);

err_shm_malloc:
	TEEC_CloseSession(&session);

err_open_sess:
	TEEC_FinalizeContext(&context);

err_init_context:

	printf("==== [%s] END ====\n", __func__);

	return result;
}

static TEEC_Result run_compare_register_buffer_test(void)
{
	printf("==== [%s] START ====\n", __func__);

	TEEC_Context context = { 0 };
	TEEC_Session session = { 0 };
	TEEC_Operation operation = { 0 };
	TEEC_SharedMemory sharedMem = { 0 };
	TEEC_Result result = TEEC_ERROR_GENERIC;
	uint32_t return_origin = 0xFFFFFFFF;
	uint8_t check_buf[BUFFER_SIZE] = { 0 };

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_InitializeContext failed, ret = 0x%x.\n", result);
		goto err_init_context;
	}

	result = TEEC_OpenSession(&context, &session, &gp_test_uuid,
				  TEEC_LOGIN_USER, NULL, NULL, &return_origin);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_OpenSession failed, ret = 0x%x.\n", result);
		goto err_open_sess;
	}

	// Allocate shared mem
	sharedMem.buffer = malloc(BUFFER_SIZE);
	if (!sharedMem.buffer)
		goto err_shm_malloc;

	sharedMem.size = BUFFER_SIZE;
	sharedMem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;

	result = TEEC_RegisterSharedMemory(&context, &sharedMem);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_RegisterSharedMemory failed, ret = 0x%x.\n",
		       result);
		goto err_register_shm;
	}

	/* Initialize buffer to all 1's */
	memset(sharedMem.buffer, 0x1, sharedMem.size);

	/* Initialize check_buf to all 1's */
	memset(check_buf, 0x1, sizeof(check_buf));

	/* Setup operation */
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
						TEEC_MEMREF_PARTIAL_INOUT,
						TEEC_NONE, TEEC_NONE);

	operation.params[0].value.a = 42;
	operation.params[1].memref.parent = &sharedMem;
	operation.params[1].memref.offset = 0;
	operation.params[1].memref.size = BUFFER_SIZE;

	result = TEEC_InvokeCommand(&session, EXAMPLE_MULTIPLY_HLOS_BUFFER_CMD,
				    &operation, &return_origin);

	if (result != TEEC_SUCCESS) {
		printf("TEEC_InvokeCommand failed, ret = 0x%x.\n", result);
		goto err_invoke_cmd;
	}

	/* Do the multiply locally */
	for (size_t cnt = 0; cnt < sharedMem.size; ++cnt)
		*(check_buf + cnt) *= operation.params[0].value.a;

	if (memcmp(sharedMem.buffer, check_buf, sharedMem.size))
		printf("[TEST FAILED] Buffer comparison failed!\n");
	else
		printf("[TEST PASSED] Buffer comparison success.\n");

err_invoke_cmd:
	TEEC_ReleaseSharedMemory(&sharedMem);

err_register_shm:
	free(sharedMem.buffer);

err_shm_malloc:
	TEEC_CloseSession(&session);

err_open_sess:
	TEEC_FinalizeContext(&context);

err_init_context:

	printf("==== [%s] END ====\n", __func__);

	return result;
}

static TEEC_Result run_multiply_alloc_buffer_test(void)
{
	printf("==== [%s] START ====\n", __func__);

	TEEC_Context context = { 0 };
	TEEC_Session session = { 0 };
	TEEC_Operation operation = { 0 };
	TEEC_SharedMemory sharedMem = { 0 };
	TEEC_Result result = TEEC_ERROR_GENERIC;
	uint32_t return_origin = 0xFFFFFFFF;

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_InitializeContext failed, ret = 0x%x.\n", result);
		goto err_init_context;
	}

	result = TEEC_OpenSession(&context, &session, &example_gpapp_ta_uuid,
				  TEEC_LOGIN_USER, NULL, NULL, &return_origin);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_OpenSession failed, ret = 0x%x.\n", result);
		goto err_open_sess;
	}

	// Allocate shared memory
	sharedMem.size = BUFFER_SIZE;
	sharedMem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;

	printf("Allocate a shared buffer: size = 0%zX, flags = 0x%x.\n",
	       sharedMem.size, sharedMem.flags);
	result = TEEC_AllocateSharedMemory(&context, &sharedMem);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_AllocateSharedMemory failed, ret = 0x%x.\n",
		       result);
		goto err_alloc_shm;
	}

	// Initialize shared buffer to all 1's
	memset(sharedMem.buffer, 0x1, sharedMem.size);

	// Setup operation
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT,
						TEEC_MEMREF_PARTIAL_INOUT,
						TEEC_NONE, TEEC_NONE);
	operation.params[0].value.a = 0x2A;
	operation.params[1].memref.parent = &sharedMem;
	operation.params[1].memref.offset = 0;
	operation.params[1].memref.size = BUFFER_SIZE;

	// Request multiply on the shared memory
	printf("Request multiplication on the shared buffer (every 1 byte) "
	       "by 0x%x\n",
	       operation.params[0].value.a);
	result = TEEC_InvokeCommand(&session, EXAMPLE_MULTIPLY_HLOS_BUFFER_CMD,
				    &operation, &return_origin);

	if (result != TEEC_SUCCESS) {
		printf("TEEC_InvokeCommand failed, ret = 0x%x.\n", result);
		goto err_invoke_cmd;
	}

	// Print one byte of the resulted buffer as an example purpose
	printf("Resulted buffer[0] = 0x%2x\n",
	       ((uint8_t const *)sharedMem.buffer)[0]);

err_invoke_cmd:
	TEEC_ReleaseSharedMemory(&sharedMem);

err_alloc_shm:
	TEEC_CloseSession(&session);

err_open_sess:
	TEEC_FinalizeContext(&context);

err_init_context:

	printf("==== [%s] END ====\n", __func__);

	return result;
}

static void *send_cancel_request(void *arg)
{
	usleep(200 * 1000);
	TEEC_RequestCancellation((TEEC_Operation *)arg);

	return 0;
}

static int run_invoke_cmd_cancellation_test(void)
{
	printf("==== [%s] START ====\n", __func__);

	/* Allocate TEE Client structures on the stack. */
	TEEC_Context context = { 0 };
	TEEC_Session session = { 0 };
	TEEC_Operation operation = { 0 };
	TEEC_Result result = 0xFFFFFFFF;
	uint32_t returnOrigin = 0;
	uint32_t command = GP_SAMPLE_WAIT_TEST;
	pthread_t cancel_thread_id;

	operation.paramTypes =
		TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	operation.started = 0;

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_InitializeContext failed, ret = 0x%x.\n", result);
		goto err_init_context;
	}

	result = TEEC_OpenSession(&context, &session, &gp_sample2_uuid,
				  TEEC_LOGIN_USER, NULL, NULL, &returnOrigin);
	if (result != TEEC_SUCCESS) {
		printf("TEEC_OpenSession failed, ret = 0x%x.\n", result);
		goto err_open_sess;
	}

	/* Create a new thread to cancel the command */
	result = pthread_create(&cancel_thread_id, NULL, send_cancel_request,
				&operation);
	if (result) {
		printf("pthread_create failed, ret = 0x%x.\n", result);
		goto err_thread_create;
	}

	operation.started = 0;

	result = TEEC_InvokeCommand(&session, command, &operation,
				    &returnOrigin);

	pthread_join(cancel_thread_id, NULL);

	if (result == TEEC_ERROR_CANCEL) {
		printf("Invoke command cancellation test passed!\n");
		result = TEEC_SUCCESS;
	} else
		printf("Invoke command cancellation test failed!, ret = 0x%x.\n",
		       result);

err_thread_create:
	TEEC_CloseSession(&session);

err_open_sess:
	TEEC_FinalizeContext(&context);

err_init_context:

	printf("==== [%s] END ====\n", __func__);

	return result;
}

static void usage(void)
{
	printf("\n\n---------------------------------------------------------\n"
	       "Usage: gp_test_client -[OPTION] [PATH TO GP TAs]\n\n"
	       "Runs basic Global Platform client tests.\n"
	       "\n\n"
	       "OPTION can be:\n"
	       "  -l, Pre-load the Global Platform TAs from the specified location.\n"
	       "      e.g. gp_test_client -l </path/to/gp_tas>\n\n"
	       "  -a  Auto-load the Global Platform TAs from the default location.\n"
	       "      e.g. gp_test_client -a\n\n\n");
}

static int parse_command(int argc, char *const argv[])
{
	int command = 0;
	unsigned int ret = -1;

	while ((command = getopt_long(argc, argv, "la", testopts, NULL)) !=
		-1) {
		switch (command) {
		case 'l':
			ret = 1;
			break;
		case 'a':
			ret = 0;
			break;
		default:
			usage();
			break;
		}
	}

	optind = 1;

	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	TEEC_Result result = TEEC_SUCCESS;
	int preload = 0;

	if (argc < 2) {
		usage();
		return -1;
	}

	preload = parse_command(argc, argv);
	if (preload == -1)
		return ret;

	/* Pre-load the GP TAs in-case TA autoload feature isn't available. */
	if (preload) {
		ret = preload_gp_tas(argv[2]);
		if (ret) {
			printf("preload_gp_tas failed: %d", ret);
			return -1;
		}
	} else {
		goto common_tests;
	}

	/* Add tests which depend on pre-loaded TAs here */
	result = run_multiply_alloc_buffer_test();
	if (result != TEEC_SUCCESS) {
		printf("run_multiply_alloc_buffer_test failed: 0x%x\n", result);
		ret = -1;
		goto exit;
	}

common_tests:

	result = run_compare_register_buffer_test();
	if (result != TEEC_SUCCESS) {
		printf("run_compare_register_buffer_test failed: 0x%x\n",
		       result);
		ret = -1;
		goto exit;
	}

	result = run_gptest_app_cmds();
	if (result != TEEC_SUCCESS) {
		printf("run_gptest_app_cmds failed: 0x%x\n", result);
		ret = -1;
		goto exit;
	}

	result = run_compare_tmp_buffer_test();
	if (result != TEEC_SUCCESS) {
		printf("run_compare_tmp_buffer_test failed: 0x%x\n", result);
		ret = -1;
		goto exit;
	}

	result = run_temp_memory_ref_test();
	if (result != TEEC_SUCCESS) {
		printf("run_temp_memory_ref_test failed: 0x%x\n", result);
		ret = -1;
		goto exit;
	}

	result = run_allocate_partial_mem_offset_test();
	if (result != TEEC_SUCCESS) {
		printf("run_allocate_partial_mem_offset_test failed: 0x%x\n",
		       result);
		ret = -1;
		goto exit;
	}

	result = run_register_partial_mem_offset_test();
	if (result != TEEC_SUCCESS) {
		printf("run_register_partial_mem_offset_test failed: 0x%x\n",
		       result);
		ret = -1;
		goto exit;
	}

	result = run_invoke_cmd_cancellation_test();
	if (result != TEEC_SUCCESS) {
		printf("run_invoke_cmd_cancellation_test failed: 0x%x\n",
		       result);
		ret = -1;\
		goto exit;
	}

exit:
	if (preload)
		unload_gp_tas();

	return ret;
}
