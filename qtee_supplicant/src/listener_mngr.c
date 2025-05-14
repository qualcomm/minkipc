// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <dlfcn.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>

#include "listener_mngr.h"
#include "MinkCom.h"
#include "qtee_supplicant.h"

#include "CListenerCBO.h"
#include "CRegisterListenerCBO.h"
#include "IRegisterListenerCBO.h"
#include "IClientEnv.h"

#define NUM_LISTENERS 1
static struct listener_svc listeners[] = {
	{
		.service_name = "time services",
		.id = TIME_SERVICE,
		.is_registered = false,
		.file_name = "libdrmtime.so.1",
		.lib_handle = NULL,
		.svc_init = NULL,
		.smci_dispatch = "smci_dispatch",
		.cbo = Object_NULL,
		.mo = Object_NULL,
		.buf_len = TIME_SERVICE_BUF_LEN,
	},
};

/* List of listener registration objects */
static Object obj_list[NUM_LISTENERS] = { Object_NULL };

/**
 * @brief Stop listener services.
 *
 * Stops all listener services waiting for a listener request from QTEE.
 */
static void stop_listeners_smci(void)
{
	size_t idx = 0;

	/* Stop the root listener services */
	MSGD("Total listener services to be stopped = %d\n", NUM_LISTENERS);

	for (idx = 0; idx < NUM_LISTENERS; idx++) {
		if (Object_isNull(obj_list[idx]))
			continue;

		Object_ASSIGN_NULL(obj_list[idx]);
	}

	for (idx = 0; idx < NUM_LISTENERS; idx++) {
		/* Resource cleanup for registered listeners */
		if(listeners[idx].is_registered) {
			Object_ASSIGN_NULL(listeners[idx].cbo);
			Object_ASSIGN_NULL(listeners[idx].mo);

			listeners[idx].is_registered = false;
		}

		/* Close lib_handle for all listeners */
		if(listeners[idx].lib_handle != NULL) {
			dlclose(listeners[idx].lib_handle);
			listeners[idx].lib_handle = NULL;
		}
	}
}

int init_listener_services(void)
{
	svc_init srv_init;
	int ret = 0;

	for (int i = 0; i < NUM_LISTENERS; i++) {

		listeners[i].lib_handle = dlopen(listeners[i].file_name,
						 RTLD_NOW);
		if (listeners[i].lib_handle == NULL) {
			MSGE("dlopen(%s, RLTD_NOW) failed: %s\n",
			     listeners[i].file_name, dlerror());

			return -1;
		}

		if (listeners[i].svc_init == NULL)
			continue;

		srv_init = (svc_init)dlsym(listeners[i].lib_handle,
					   listeners[i].svc_init);
		if (srv_init == NULL) {
			MSGE("dlsym(%s) not found in lib %s: %s\n",
			     listeners[i].svc_init, listeners[i].file_name,
			     dlerror());

			return -1;
		}

		ret = (*srv_init)();
		if (ret < 0) {
			MSGE("Init for dlsym(%s) failed: %d",
			     listeners[i].svc_init, ret);

			return -1;
		}
	}

	return 0;
}

int start_listener_services(void)
{
	int32_t rv = Object_OK;
	int ret = 0;
	size_t idx = 0;

	Object root = Object_NULL;
	Object client_env = Object_NULL;
	Object register_obj = Object_NULL;

	MSGD("Total listener services to start = %d\n", NUM_LISTENERS);

	for (idx = 0; idx < NUM_LISTENERS; idx++) {

		/* There are 4 threads for each callback. */
		rv = MinkCom_getRootEnvObject(&root);
		if (Object_isERROR(rv)) {
			MSGE("getRootEnvObject failed: 0x%x\n", rv);
			ret = -1;
			goto exit_release;
		}

		rv = MinkCom_getClientEnvObject(root, &client_env);
		if (Object_isERROR(rv)) {
			Object_ASSIGN_NULL(root);
			MSGE("getClientEnvObject failed: 0x%x\n", rv);
			ret = -1;
			goto exit_release;
		}

		rv = IClientEnv_open(client_env, CRegisterListenerCBO_UID,
				     &register_obj);
		if (Object_isERROR(rv)) {
			Object_ASSIGN_NULL(client_env);
			Object_ASSIGN_NULL(root);
			MSGE("IClientEnv_open failed: 0x%x\n", rv);
			ret = -1;
			goto exit_release;
		}

		Object_ASSIGN(obj_list[idx], register_obj);
		Object_ASSIGN_NULL(register_obj);

		rv = MinkCom_getMemoryObject(root, listeners[idx].buf_len,
					     &listeners[idx].mo);
		if (Object_isERROR(rv)) {
			Object_ASSIGN_NULL(client_env);
			Object_ASSIGN_NULL(root);
			ret = -1;
			MSGE("getMemoryObject failed: 0x%x", rv);
			goto exit_release_obj;
		}

		/* Create CBO listener and register it */
		rv = CListenerCBO_new(&listeners[idx].cbo, listeners[idx].mo,
				      &listeners[idx]);
		if (Object_isERROR(rv)) {
			Object_ASSIGN_NULL(client_env);
			Object_ASSIGN_NULL(root);
			ret = -1;
			MSGE("CListenerCBO_new failed: 0x%x\n", rv);
			goto exit_release_mo;
		}

		rv = IRegisterListenerCBO_register(obj_list[idx],
						   listeners[idx].id,
						   listeners[idx].cbo,
						   listeners[idx].mo);
		if (Object_isERROR(rv)) {
			Object_ASSIGN_NULL(client_env);
			Object_ASSIGN_NULL(root);
			ret = -1;
			MSGE("IRegisterListenerCBO_register(%d) failed: 0x%x",
			     listeners[idx].id, rv);
			goto exit_release_cbo;
		}

		listeners[idx].is_registered = true;

		Object_ASSIGN_NULL(client_env);
		Object_ASSIGN_NULL(root);
	}

	return ret;

exit_release_cbo:
	Object_ASSIGN_NULL(listeners[idx].cbo);

exit_release_mo:
	Object_ASSIGN_NULL(listeners[idx].mo);

exit_release_obj:
	Object_ASSIGN_NULL(obj_list[idx]);

exit_release:
	stop_listeners_smci();

	return ret;
}
