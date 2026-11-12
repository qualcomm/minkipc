// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * fsp_client — Factory Safe Provisioning (FSP) client
 *
 * Loads the fspapp Trusted Application into QTEE via IAppLoader and invokes
 * its IFSPApp interface to retrieve a public key or provision a key.
 */

#include <getopt.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
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
#include "object.h"

using namespace std;

/* Directory holding the TA binaries; can be overridden with -DTA_DIR=<path>. */
#ifndef TA_PATH
#define TA_PATH "/usr/lib/qtee-tas"
#endif

static void fsp_client_help_usage(void)
{
	printf("Usage: fsp_client -a <app_name> -c <cmd_id> [OPTIONS]\n\n"
	       "Options:\n"
	       "  -a, --app_name <name>     Name of the secure app\n"
	       "  -c, --cmd_id <id>         Command ID:\n"
	       "                              1 - get_public_key\n"
	       "                              2 - provision_key\n"
	       "  -d, --feature_id <id>     Feature ID of the key\n"
	       "  -f, --file <path>         Keybox file (required for provision_key)\n"
	       "  -e, --encrypted <0|1>     Keybox encryption status (0=clear, 1=encrypted)\n"
	       "  -h, --help                Show this help message\n");
}

/* Read the whole file at `path` into `vec`. */
static int32_t read_file(vector<uint8_t> &vec, const string &path)
{
	ifstream file(path, ios::binary | ios::ate);
	streamsize length;

	if (!file.is_open()) {
		LOG_ERR("failed to open %s\n", path.c_str());
		return Object_ERROR;
	}

	length = file.tellg();
	if (length <= 0) {
		LOG_ERR("empty or unreadable file: %s\n", path.c_str());
		return Object_ERROR;
	}

	vec.resize(static_cast<size_t>(length));
	file.seekg(0, ios::beg);
	if (!file.read(reinterpret_cast<char *>(vec.data()), length)) {
		LOG_ERR("failed to read %s\n", path.c_str());
		return Object_ERROR;
	}

	return Object_OK;
}

/* Feature IDs for which the TA accepts a provisioned key. */
static bool is_valid_provision_feature_id(uint32_t feature_id)
{
	return feature_id == UIE_KEY_0_FEATURE_ID ||
	       feature_id == UIE_KEY_1_FEATURE_ID;
}

/* Push "<TA_PATH>/<app_name>.mbn" to QTEE and fetch its IFSPApp object. */
static int32_t load_trusted_app(Object root, const char *app_name,
				fsp_usecase_handler *handler)
{
	int32_t rv;
	Object client_env = Object_NULL;
	Object app_loader = Object_NULL;
	vector<uint8_t> elf;
	string path = string(TA_PATH) + "/" + app_name + ".mbn";

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

	rv = read_file(elf, path);
	if (Object_isERROR(rv))
		goto cleanup;

	rv = IAppLoader_loadFromBuffer(app_loader, elf.data(), elf.size(),
				       &handler->app_controller);
	if (Object_isERROR(rv)) {
		handler->app_controller = Object_NULL;
		LOG_ERR("IAppLoader_loadFromBuffer(%s) failed: 0x%x\n",
			path.c_str(), rv);
		goto cleanup;
	}

	rv = IAppController_getAppObject(handler->app_controller,
					 &handler->fsp_ta);
	if (Object_isERROR(rv)) {
		handler->fsp_ta = Object_NULL;
		LOG_ERR("IAppController_getAppObject failed: %d\n", rv);
	}

cleanup:
	Object_ASSIGN_NULL(app_loader);
	Object_ASSIGN_NULL(client_env);
	return rv;
}

static void unload_trusted_app(fsp_usecase_handler *handler)
{
	int32_t rv;

	Object_ASSIGN_NULL(handler->fsp_ta);

	if (!Object_isNull(handler->app_controller)) {
		rv = IAppController_unload(handler->app_controller);
		if (Object_isERROR(rv))
			LOG_ERR("IAppController_unload failed: %d\n", rv);
		Object_ASSIGN_NULL(handler->app_controller);
	}
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

	rv = MinkCom_getMemoryObjectInfo(buf.memory_obj, &buf.cpu_va, &buf.size);
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

static int32_t get_public_key(const fsp_usecase_handler &handler,
			      const char *app_name, uint32_t feature_id)
{
	int32_t rv;
	uint8_t pub_key[64] = { 0 };
	size_t pub_key_len = 0;

	rv = IFSPApp_get_public_key(handler.fsp_ta, feature_id, pub_key,
				    sizeof(pub_key), &pub_key_len);
	if (Object_isERROR(rv)) {
		LOG_ERR("IFSPApp_get_public_key failed: 0x%x\n", rv);
		return rv;
	}

	printf("ECC Public Key for %s TA: ", app_name);
	for (size_t i = 0; i < pub_key_len; i++)
		printf("%02x", pub_key[i]);
	printf("\n");

	return Object_OK;
}

static int32_t provision_key(const fsp_usecase_handler &handler, Object root,
			     uint32_t feature_id, const string &in_file,
			     uint32_t is_encrypted)
{
	int32_t rv;
	vector<uint8_t> vec;
	fsp_buf_t in_buf = {};

	if (!is_valid_provision_feature_id(feature_id)) {
		LOG_ERR("invalid feature_id for provision_key\n");
		return Object_ERROR;
	}

	if (in_file.empty()) {
		LOG_ERR("no input file provided\n");
		return Object_ERROR;
	}

	rv = read_file(vec, in_file);
	if (Object_isERROR(rv))
		return rv;

	rv = allocate_buffer(in_buf, root, vec.size());
	if (Object_isERROR(rv))
		return rv;

	memcpy(in_buf.cpu_va, vec.data(), vec.size());

	rv = IFSPApp_decrypt_and_provision_key(handler.fsp_ta, feature_id,
					       in_buf.memory_obj,
					       (uint32_t)vec.size(), is_encrypted);
	if (Object_isERROR(rv))
		LOG_ERR("IFSPApp_decrypt_and_provision_key failed: 0x%x\n", rv);
	else
		printf("Key provisioned successfully!\n");

	deallocate_buffer(in_buf);
	return rv;
}

int main(int argc, char *argv[])
{
	int32_t rv;
	int32_t cmd_id = -1;
	uint32_t is_encrypted = 1;
	uint32_t feature_id = UINT32_MAX;
	const char *app_name = nullptr;
	string in_file;
	int opt;
	Object root = Object_NULL;
	fsp_usecase_handler handler = { Object_NULL, Object_NULL };

	static struct option long_options[] = {
		{ "help", no_argument, 0, 'h' },
		{ "app_name", required_argument, 0, 'a' },
		{ "cmd_id", required_argument, 0, 'c' },
		{ "file", required_argument, 0, 'f' },
		{ "encrypted", required_argument, 0, 'e' },
		{ "feature_id", required_argument, 0, 'd' },
		{ 0, 0, 0, 0 },
	};

	while ((opt = getopt_long(argc, argv, "ha:c:f:e:d:", long_options,
				  nullptr)) != -1) {
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
			is_encrypted = strtoul(optarg, nullptr, 0) ? 1 : 0;
			break;
		case 'd':
			feature_id = (uint32_t)strtoul(optarg, nullptr, 0);
			break;
		default:
			fsp_client_help_usage();
			return -1;
		}
	}

	if (app_name == nullptr) {
		printf("Error: app name is required. Use -a <app_name>.\n");
		fsp_client_help_usage();
		return -1;
	}

	if (cmd_id != FSP_GET_PUBLIC_KEY && cmd_id != FSP_PROVISION_KEY) {
		printf("Error: a valid cmd_id is required. Use -c <cmd_id>.\n");
		fsp_client_help_usage();
		return -1;
	}

	rv = MinkCom_getRootEnvObject(&root);
	if (Object_isERROR(rv)) {
		LOG_ERR("MinkCom_getRootEnvObject failed: %d\n", rv);
		return -1;
	}

	rv = load_trusted_app(root, app_name, &handler);
	if (Object_isERROR(rv))
		goto cleanup;

	if (cmd_id == FSP_GET_PUBLIC_KEY)
		rv = get_public_key(handler, app_name, feature_id);
	else
		rv = provision_key(handler, root, feature_id, in_file,
				   is_encrypted);

cleanup:
	unload_trusted_app(&handler);
	Object_ASSIGN_NULL(root);
	return Object_isERROR(rv) ? -1 : 0;
}
