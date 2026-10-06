// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __RPMB_SERVICE_H__
#define __RPMB_SERVICE_H__

#include <stdint.h>

/*
 * Shared-memory buffer size (must match TZ expectation)
 */

#define TZ_CM_MAX_DATA_LEN 20000
#define TZ_MAX_BUF_LEN (TZ_CM_MAX_DATA_LEN + 40)

/* RPMB listener service ID */
#define RPMB_SERVICE_ID 0x2000

/*
 * TZ <-> HLOS command IDs
 */

typedef enum {
	TZ_CM_CMD_RPMB_INIT = 0x101, /* device init */
	TZ_CM_CMD_RPMB_READ = 0x102, /* authenticated read */
	TZ_CM_CMD_RPMB_WRITE = 0x103, /* authenticated write */
	TZ_CM_CMD_RPMB_PARTITION = 0x104, /* partition table query */
} tz_rpmb_cmd_id_t;

/*
 * RPMB init message versions
 */

#define RPMB_LSTNR_VERSION_1 1
#define RPMB_LSTNR_VERSION_2 2

/*
 * TZ request / response wire structures
 */

typedef struct {
	uint32_t cmd_id;
	uint32_t version;
} __attribute__((packed)) tz_sd_device_init_req_t;

typedef struct {
	uint32_t cmd_id;
	uint32_t version;
	int32_t status;
	uint32_t num_sectors;
	uint32_t rel_wr_count;
} __attribute__((packed)) tz_sd_device_init_res_t;

typedef struct {
	uint32_t cmd_id;
	uint32_t version;
	int32_t status;
	uint32_t num_sectors;
	uint32_t rel_wr_count;
	uint32_t dev_type;
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
} __attribute__((packed)) tz_sd_device_init_res_v02_t;

typedef struct {
	uint32_t cmd_id;
	uint32_t num_sectors;
	uint32_t req_buff_len;
	uint32_t req_buff_offset;
	uint32_t version;
	uint32_t rel_wr_count;
} __attribute__((packed)) tz_rpmb_rw_req_t;

typedef struct {
	uint32_t cmd_id;
	int32_t status;
	uint32_t res_buff_len;
	uint32_t res_buff_offset;
	uint32_t version;
} __attribute__((packed)) tz_rpmb_rw_res_t;

typedef struct {
	uint32_t cmd_id;
	uint32_t version;
	uint32_t dev_id;
} __attribute__((packed)) tz_sd_rpmb_partition_req_t;

typedef struct {
	uint32_t cmd_id;
	uint32_t status;
	uint32_t num_partitions;
	uint32_t rsp_buff_offset;
} __attribute__((packed)) tz_sd_rpmb_partition_rsp_t;

/*
 * Partition table version constants
 */

#define RPMB_LSTNR_PARTI_TABLE_VER_1 0x100
#define RPMB_LSTNR_PARTI_TABLE_VER_2 0x200

#endif /* __RPMB_SERVICE_H__ */
