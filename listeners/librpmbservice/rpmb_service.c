// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
 * RPMB MinkIPC listener service.
 *
 * Registers with QTEE as a CBO listener for RPMB_SERVICE_ID.  On each
 * dispatch call from the secure world it decodes the command ID and
 * forwards to the appropriate handler:
 *
 *   TZ_CM_CMD_RPMB_INIT      (0x101) -- device init / capability query
 *   TZ_CM_CMD_RPMB_READ      (0x102) -- authenticated read
 *   TZ_CM_CMD_RPMB_WRITE     (0x103) -- authenticated write
 *   TZ_CM_CMD_RPMB_PARTITION (0x104) -- partition table query
 *
 * All storage I/O is delegated to rpmb.c which selects the active device
 * (eMMC or UFS) at init time via the registered driver table.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rpmb.h"
#include "rpmb_logging.h"
#include "rpmb_service.h"

#include "CListenerCBO.h"
#include "CRegisterListenerCBO.h"
#include "IRegisterListenerCBO.h"
#include "IClientEnv.h"
#include "MinkCom.h"

/* Exported symbols called by the shared-library loader */
int init(void);
void deinit(void);

/* Dispatch callback registered with MinkIPC */
int smci_dispatch(void *buf, size_t buf_len);

/* MinkIPC objects -- module-level lifetime */
static Object register_obj = Object_NULL;
static Object mo = Object_NULL;
static Object cbo = Object_NULL;

/*
 * Command handlers
 */

static int rpmb_handle_init(void *req, void *rsp)
{
	tz_sd_device_init_req_t *init_req = (tz_sd_device_init_req_t *)req;
	tz_sd_device_init_res_t init_rsp = {0};
	tz_sd_device_init_res_v02_t init_rsp_v02 = {0};
	rpmb_init_info_t info = {0};

	uint32_t cmd_id = init_req->cmd_id;
	uint32_t version = init_req->version;
	int status = rpmb_init(&info);

	RPMB_LOG_INFO("RPMB init: status=%d size=%u rel_wr=%u type=%u\n",
		      status, info.size, info.rel_wr_count, info.dev_type);

	if (version == RPMB_LSTNR_VERSION_1) {
		init_rsp.cmd_id = cmd_id;
		init_rsp.version = RPMB_LSTNR_VERSION_1;
		init_rsp.status = status;
		init_rsp.num_sectors = info.size;
		init_rsp.rel_wr_count = info.rel_wr_count;
		memmove(rsp, &init_rsp, sizeof(init_rsp));
	} else if (version >= RPMB_LSTNR_VERSION_2) {
		init_rsp_v02.cmd_id = cmd_id;
		init_rsp_v02.version = RPMB_LSTNR_VERSION_2;
		init_rsp_v02.status = status;
		init_rsp_v02.num_sectors = info.size;
		init_rsp_v02.rel_wr_count = info.rel_wr_count;
		init_rsp_v02.dev_type = info.dev_type;
		memmove(rsp, &init_rsp_v02, sizeof(init_rsp_v02));
	} else {
		RPMB_LOG_ERROR("RPMB init: unsupported version=%u\n", version);
		init_rsp.cmd_id = cmd_id;
		init_rsp.version = RPMB_LSTNR_VERSION_1;
		init_rsp.status = -1;
		init_rsp.num_sectors = 0;
		init_rsp.rel_wr_count = 0;
		memmove(rsp, &init_rsp, sizeof(init_rsp));
	}
	return 0;
}

static int rpmb_handle_read(void *req, void *rsp)
{
	tz_rpmb_rw_req_t *rw_req = (tz_rpmb_rw_req_t *)req;
	tz_rpmb_rw_res_t *rw_rsp = (tz_rpmb_rw_res_t *)rsp;
	void *rpmb_req = (uint8_t *)req + rw_req->req_buff_offset;
	void *rpmb_resp = (uint8_t *)rsp + sizeof(*rw_rsp);
	uint32_t resp_len = 0;

	rw_rsp->status = rpmb_read(rpmb_req, rw_req->num_sectors,
				   rpmb_resp, &resp_len);
	rw_rsp->res_buff_len = resp_len;
	rw_rsp->res_buff_offset = sizeof(*rw_rsp);
	rw_rsp->cmd_id = rw_req->cmd_id;
	rw_rsp->version = rw_req->version;

	if (rw_rsp->status != 0)
		RPMB_LOG_ERROR("RPMB read failed: status=%d\n", rw_rsp->status);
	return 0;
}

static int rpmb_handle_write(void *req, void *rsp)
{
	tz_rpmb_rw_req_t *rw_req = (tz_rpmb_rw_req_t *)req;
	tz_rpmb_rw_res_t *rw_rsp = (tz_rpmb_rw_res_t *)rsp;
	void *rpmb_req = (uint8_t *)req + rw_req->req_buff_offset;
	void *rpmb_resp = (uint8_t *)rsp + sizeof(*rw_rsp);
	uint32_t resp_len = 0;

	RPMB_LOG_INFO("RPMB write: %u sectors\n", rw_req->num_sectors);

	rw_rsp->status = rpmb_write(rpmb_req, rw_req->num_sectors,
				    rpmb_resp, &resp_len,
				    rw_req->rel_wr_count);
	rw_rsp->res_buff_len = resp_len;
	rw_rsp->res_buff_offset = sizeof(*rw_rsp);
	rw_rsp->cmd_id = rw_req->cmd_id;
	rw_rsp->version = rw_req->version;

	if (rw_rsp->status != 0)
		RPMB_LOG_ERROR("RPMB write failed: status=%d\n",
			       rw_rsp->status);
	return 0;
}

static int rpmb_handle_partition(void *req, void *rsp)
{
	tz_sd_rpmb_partition_req_t *p_req = (tz_sd_rpmb_partition_req_t *)req;
	tz_sd_rpmb_partition_rsp_t *p_rsp = (tz_sd_rpmb_partition_rsp_t *)rsp;

	RPMB_LOG_INFO("RPMB partition: version=0x%x dev_id=%u\n",
		      p_req->version, p_req->dev_id);

	p_rsp->cmd_id = p_req->cmd_id;
	p_rsp->num_partitions = 0;
	p_rsp->rsp_buff_offset = sizeof(*p_rsp);

	if (p_req->version == RPMB_LSTNR_PARTI_TABLE_VER_1) {
		/* Version 1 partition table not provisioned on this device */
		p_rsp->status = -1;
	} else {
		RPMB_LOG_WARN("Unsupported partition table version: 0x%x\n",
			      p_req->version);
		p_rsp->status = -1;
	}
	return 0;
}

/*
 * MinkIPC dispatch callback
 */

int smci_dispatch(void *buf, size_t buf_len)
{
	uint32_t cmd_id = *(uint32_t *)buf;

	RPMB_LOG_INFO("RPMB dispatch: cmd_id=%u (0x%x) buf_len=%zu\n",
		      cmd_id, cmd_id, buf_len);

	if (buf_len < TZ_MAX_BUF_LEN) {
		RPMB_LOG_ERROR("Buffer too small: %zu < %d\n",
			       buf_len, TZ_MAX_BUF_LEN);
		return -1;
	}

	switch ((tz_rpmb_cmd_id_t)cmd_id) {
	case TZ_CM_CMD_RPMB_INIT:
		return rpmb_handle_init(buf, buf);
	case TZ_CM_CMD_RPMB_READ:
		return rpmb_handle_read(buf, buf);
	case TZ_CM_CMD_RPMB_WRITE:
		return rpmb_handle_write(buf, buf);
	case TZ_CM_CMD_RPMB_PARTITION:
		return rpmb_handle_partition(buf, buf);
	default:
		RPMB_LOG_ERROR("Unknown RPMB command: %u (0x%x)\n",
			       cmd_id, cmd_id);
		return -1;
	}
}

/*
 * Service lifecycle
 */

int init(void)
{
	int32_t rv;
	Object root = Object_NULL;
	Object client_env = Object_NULL;
	void *buf = NULL;
	size_t buf_len = 0;
	rpmb_init_info_t info = {0};

	rpmb_log_init();
	RPMB_LOG_INFO("RPMB service initializing\n");

	rv = MinkCom_getRootEnvObject(&root);
	if (Object_isERROR(rv)) {
		RPMB_LOG_ERROR("getRootEnvObject failed: 0x%x\n", rv);
		goto err;
	}

	rv = MinkCom_getClientEnvObject(root, &client_env);
	if (Object_isERROR(rv)) {
		RPMB_LOG_ERROR("getClientEnvObject failed: 0x%x\n", rv);
		goto err;
	}

	rv = IClientEnv_open(client_env, CRegisterListenerCBO_UID,
			     &register_obj);
	if (Object_isERROR(rv)) {
		RPMB_LOG_ERROR("IClientEnv_open(CRegisterListenerCBO) failed:"
			       " 0x%x\n", rv);
		goto err;
	}

	rv = MinkCom_getMemoryObject(root, TZ_MAX_BUF_LEN, &mo);
	if (Object_isERROR(rv)) {
		RPMB_LOG_ERROR("getMemoryObject failed: 0x%x\n", rv);
		goto err;
	}

	rv = MinkCom_getMemoryObjectInfo(mo, &buf, &buf_len);
	if (Object_isERROR(rv)) {
		RPMB_LOG_ERROR("getMemoryObjectInfo failed: 0x%x\n", rv);
		goto err;
	}

	rv = CListenerCBO_new(&cbo, RPMB_SERVICE_ID, smci_dispatch,
			      buf, buf_len);
	if (Object_isERROR(rv)) {
		RPMB_LOG_ERROR("CListenerCBO_new failed: 0x%x\n", rv);
		goto err;
	}

	rv = IRegisterListenerCBO_register(register_obj, RPMB_SERVICE_ID,
					   cbo, mo);
	if (Object_isERROR(rv)) {
		RPMB_LOG_ERROR("IRegisterListenerCBO_register(%d) failed:"
			       " 0x%x\n",
			       RPMB_SERVICE_ID, rv);
		goto err;
	}

	Object_ASSIGN_NULL(client_env);
	Object_ASSIGN_NULL(root);

	/* Detect and initialise storage device at startup */
	if (rpmb_init(&info) == 0) {
		RPMB_LOG_INFO("RPMB device ready: dev_type=%u size=%u"
			      " rel_wr_count=%u\n",
			      info.dev_type, info.size, info.rel_wr_count);
	} else {
		RPMB_LOG_WARN("No RPMB storage device found at startup\n");
	}

	RPMB_LOG_INFO("RPMB service registered (service ID: 0x%x)\n",
		      RPMB_SERVICE_ID);
	return 0;

err:
	Object_ASSIGN_NULL(cbo);
	Object_ASSIGN_NULL(mo);
	Object_ASSIGN_NULL(register_obj);
	Object_ASSIGN_NULL(client_env);
	Object_ASSIGN_NULL(root);
	return -1;
}

void deinit(void)
{
	RPMB_LOG_INFO("RPMB service deinitializing\n");
	Object_ASSIGN_NULL(register_obj);
	Object_ASSIGN_NULL(cbo);
	Object_ASSIGN_NULL(mo);
	rpmb_log_cleanup();
}
