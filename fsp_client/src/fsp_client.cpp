// Copyright (c) 2026, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * fsp_client — Factory Safe Provisioning (FSP) client
 *
 * MinkIPC port of the downstream fsp_client. Loads the fspapp Trusted
 * Application via IAppLoader_loadFromBuffer() and invokes IFSPApp methods
 * on it.
 */

#include <getopt.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "CAppLoader.h"
#include "CFSPApp.h"
#include "IAppController.h"
#include "IAppLoader.h"
#include "IClientEnv.h"
#include "IFSPApp.h"
#include "MinkCom.h"
#include "fsp_internal.h"
#include "fsp_util.h"
#include "object.h"

using namespace std;

/* Directories searched for "<app_name>.mbn", in priority order. */
static const char *location[] = {
	"/vendor/firmware_mnt/image", "/vendor/firmware",
	"/vendor/etc/firmware",       "/firmware/image",
	"/system/etc/firmware",       "/data",
	"/tmp",
};

static fsp_usecase_handler g_ta_handler;

static void fsp_client_help_usage(void)
{
	printf(
		"**********************************************************************\n"
		"***                     FSP CLIENT USAGE                          ***\n"
		"**********************************************************************\n"
		"Usage: fsp_client [OPTIONS]\n\n"
		"Options:\n"
		"  -a, --app_name <name>     Name of the secure app\n"
		"  -c, --cmd_id <id>         Command ID:\n"
		"                              1 - get_public_key\n"
		"                              2 - provision_key\n"
		"                              3 - verify_key\n"
		"                              4 - get_public_key_v2\n"
		"                              5 - provision_key_v2\n"
		"  -f, --file <path>         Input file path\n"
		"  -e, --encrypted <0|1>     File encryption status (0=clear, 1=encrypted)\n"
		"  -d, --feature_id <id>     Feature ID for v2 operations\n"
		"  -h, --help                Show this help message\n"
		"----------------------------------------------------------------------\n\n");
}

/*
 * find_and_load_app - search `location[]` for "<app_name>.mbn" and push it
 * to QTEE with IAppLoader_loadFromBuffer().
 */
static int32_t find_and_load_app(Object app_loader, const char *app_name,
				  Object *app_controller)
{
	int32_t rv = Object_ERROR;
	vector<uint8_t> elf;
	char path[256];

	for (uint32_t i = 0; i < C_LENGTHOF(location); i++) {
		snprintf(path, sizeof(path), "%s/%s.mbn", location[i], app_name);

		elf.clear();
		if (fsp_read_file(elf, path) != Object_OK)
			continue;

		rv = IAppLoader_loadFromBuffer(app_loader, elf.data(), elf.size(),
						app_controller);
		if (!Object_isERROR(rv)) {
			printf("Loaded %s from %s\n", app_name, path);
			return Object_OK;
		}

		LOG_ERR("IAppLoader_loadFromBuffer(%s) failed: 0x%x\n", path, rv);
	}

	LOG_ERR("Failed to find/load %s.mbn in any known location\n", app_name);
	return rv;
}

/*
 * load_trusted_app - load the fspapp TA and fetch its IFSPApp object.
 */
static int32_t load_trusted_app(const char *app_name)
{
	int32_t rv;
	Object root = Object_NULL;
	Object client_env = Object_NULL;
	Object app_loader = Object_NULL;

	rv = MinkCom_getRootEnvObject(&root);
	if (Object_isERROR(rv)) {
		root = Object_NULL;
		LOG_ERR("MinkCom_getRootEnvObject failed: %d\n", rv);
		goto cleanup;
	}

	rv = MinkCom_getClientEnvObject(root, &client_env);
	if (Object_isERROR(rv)) {
		client_env = Object_NULL;
		LOG_ERR("MinkCom_getClientEnvObject failed: %d\n", rv);
		goto cleanup;
	}

	rv = IClientEnv_open(client_env, CAppLoader_UID, &app_loader);
	if (Object_isERROR(rv)) {
		app_loader = Object_NULL;
		LOG_ERR("IClientEnv_open(CAppLoader_UID) failed: %d\n", rv);
		goto cleanup;
	}

	rv = find_and_load_app(app_loader, app_name, &g_ta_handler.app_controller);
	if (Object_isERROR(rv)) {
		g_ta_handler.app_controller = Object_NULL;
		goto cleanup;
	}

	rv = IAppController_getAppObject(g_ta_handler.app_controller,
					 &g_ta_handler.fsp_ta);
	if (Object_isERROR(rv)) {
		g_ta_handler.fsp_ta = Object_NULL;
		LOG_ERR("IAppController_getAppObject failed: %d\n", rv);
		goto cleanup;
	}

cleanup:
	Object_ASSIGN_NULL(app_loader);
	Object_ASSIGN_NULL(client_env);
	Object_ASSIGN_NULL(root);
	return rv;
}

/*
 * unload_trusted_app - release the fspapp TA handles.
 */
static int32_t unload_trusted_app(void)
{
	int32_t rv = Object_OK;

	Object_ASSIGN_NULL(g_ta_handler.fsp_ta);

	if (!Object_isNull(g_ta_handler.app_controller)) {
		rv = IAppController_unload(g_ta_handler.app_controller);
		if (Object_isERROR(rv))
			LOG_ERR("IAppController_unload failed: %d\n", rv);
		Object_ASSIGN_NULL(g_ta_handler.app_controller);
	}

	return rv;
}

static int32_t allocate_buffer(fsp_buf_t &buf, Object root, size_t size)
{
	int32_t rv;

	buf.size = size;
	rv = MinkCom_getMemoryObject(root, size, &buf.memory_obj);
	if (Object_isERROR(rv)) {
		buf.memory_obj = Object_NULL;
		LOG_ERR("MinkCom_getMemoryObject failed: %d\n", rv);
		return rv;
	}

	rv = MinkCom_getMemoryObjectInfo(buf.memory_obj, (void **)&buf.cpu_va,
					 &buf.size);
	if (Object_isERROR(rv)) {
		LOG_ERR("MinkCom_getMemoryObjectInfo failed: %d\n", rv);
		Object_ASSIGN_NULL(buf.memory_obj);
	}

	return rv;
}

static void deallocate_buffer(fsp_buf_t &buf)
{
	Object_ASSIGN_NULL(buf.memory_obj);
	buf.cpu_va = nullptr;
}

static int32_t get_public_key(const char *app_name)
{
	(void)app_name;
	printf("get_public_key not supported for this target\n");
	return Object_ERROR;
}

static int32_t provision_key(const char *app_name, const string &in_file,
			     int32_t is_encrypted)
{
	(void)app_name;
	(void)in_file;
	(void)is_encrypted;
	printf("provision_key not supported for this target\n");
	return Object_ERROR;
}

static int32_t verify_key(const char *app_name)
{
	(void)app_name;
	printf("verify_key not supported for this target\n");
	return Object_ERROR;
}

static int32_t get_public_key_v2(const char *app_name, uint32_t feature_id)
{
	int32_t rv;
	uint8_t pub_key[64] = { 0 };
	size_t pub_key_len = sizeof(pub_key);

	rv = IFSPApp_get_public_key(g_ta_handler.fsp_ta, feature_id, pub_key,
				    pub_key_len, &pub_key_len);
	if (Object_isERROR(rv)) {
		LOG_ERR("IFSPApp_get_public_key failed: 0x%x\n", rv);
		return rv;
	}

	printf("ECC Public Key for %s TA: ", app_name);
	for (size_t i = 0; i < pub_key_len; i++)
		printf("%02hhx", (unsigned char)pub_key[i]);
	printf("\n");

	return Object_OK;
}

static int32_t provision_key_v2(uint32_t feature_id, const string &in_file,
				int32_t is_encrypted, Object root)
{
	int32_t rv;
	vector<uint8_t> vec;
	fsp_buf_t in_buf = {};
	uint32_t buf_len_out = 0;

	if (!fsp_is_valid_provision_v2_feature_id(feature_id)) {
		printf("Please enter a valid feature_id to provision the key...\n");
		fsp_client_help_usage();
		return Object_ERROR;
	}

	if (in_file.empty()) {
		LOG_ERR("no input file provided\n");
		return Object_ERROR;
	}

	rv = fsp_read_file(vec, in_file);
	if (Object_isERROR(rv))
		return rv;

	rv = allocate_buffer(in_buf, root, vec.size());
	if (Object_isERROR(rv))
		return rv;

	memcpy(in_buf.cpu_va, vec.data(), vec.size());

	rv = IFSPApp_decrypt_and_provision_key(g_ta_handler.fsp_ta, feature_id,
					       in_buf.memory_obj,
					       (uint32_t)vec.size(),
					       (uint32_t)is_encrypted);
	(void)buf_len_out;
	if (Object_isERROR(rv))
		LOG_ERR("IFSPApp_decrypt_and_provision_key failed: 0x%x\n", rv);
	else
		printf("Key provisioned successfully!\n");

	deallocate_buffer(in_buf);
	return rv;
}

int main(int argc, char *argv[])
{
	int32_t rv = Object_ERROR;
	int32_t cmd_id = -1;
	int32_t is_encrypted = 1;
	int32_t feature_id = -1;
	const char *app_name = nullptr;
	string in_file;
	int opt;
	int long_index = 0;
	Object root = Object_NULL;

	static struct option long_options[] = {
		{ "help", no_argument, 0, 'h' },
		{ "app_name", required_argument, 0, 'a' },
		{ "cmd_id", required_argument, 0, 'c' },
		{ "file", required_argument, 0, 'f' },
		{ "encrypted", required_argument, 0, 'e' },
		{ "feature_id", required_argument, 0, 'd' },
		{ 0, 0, 0, 0 },
	};

	printf("-------------FSP Client------------\n");

	while ((opt = getopt_long(argc, argv, "ha:c:f:e:d:", long_options,
				   &long_index)) != -1) {
		switch (opt) {
		case 'h':
			fsp_client_help_usage();
			return 0;
		case 'a':
			app_name = optarg;
			break;
		case 'c':
			cmd_id = atoi(optarg);
			break;
		case 'f':
			in_file = optarg;
			break;
		case 'e':
			is_encrypted = atoi(optarg);
			break;
		case 'd':
			feature_id = atoi(optarg);
			break;
		case '?':
			printf("Please verify the arguments to be processed, exiting!\n");
			fsp_client_help_usage();
			return -1;
		default:
			break;
		}
	}

	if (app_name == nullptr && optind < argc && strlen(argv[optind]) > 0)
		app_name = argv[optind];

	if (app_name == nullptr) {
		printf("Error: App name is required. Use -a <app_name>.\n");
		fsp_client_help_usage();
		return -1;
	}

	rv = MinkCom_getRootEnvObject(&root);
	if (Object_isERROR(rv)) {
		LOG_ERR("MinkCom_getRootEnvObject failed: %d\n", rv);
		return -1;
	}

	rv = load_trusted_app(app_name);
	if (Object_isERROR(rv))
		goto cleanup;

	switch (cmd_id) {
	case FSP_GET_PUBLIC_KEY:
		rv = get_public_key(app_name);
		break;
	case FSP_PROVISION_KEY:
		rv = provision_key(app_name, in_file, is_encrypted);
		break;
	case FSP_VERIFY_KEY:
		rv = verify_key(app_name);
		break;
	case FSP_GET_PUBLIC_KEY_V2:
		rv = get_public_key_v2(app_name, feature_id);
		break;
	case FSP_PROVISION_KEY_V2:
		rv = provision_key_v2(feature_id, in_file, is_encrypted, root);
		break;
	default:
		printf("Please enter a valid cmd_id...\n");
		fsp_client_help_usage();
		rv = Object_ERROR;
	}

	unload_trusted_app();

cleanup:
	Object_ASSIGN_NULL(root);
	return Object_isERROR(rv) ? -1 : 0;
}
