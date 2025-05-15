// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#ifndef __TIME_MSG_H_
#define __TIME_MSG_H_

#include <stdint.h>
#include <stdio.h>

#define MSGV printf
#define MSGD printf
#define MSGE printf

/* Fixed. Don't increase the size of TZ_CM_MAX_NAME_LEN */
#define TZ_CM_MAX_NAME_LEN          256
#define TZ_CM_MAX_DATA_LEN          20000

#define TZ_MAX_BUF_LEN              (TZ_CM_MAX_DATA_LEN + 40)

#define UNUSED(x) (void)(x)

typedef struct tz_time_spec
{
	uint32_t tv_sec;  /* seconds */
	uint32_t tv_nsec; /* nanoseconds */
} __attribute__ ((packed)) tz_time_spec_t;

typedef struct tz_time
{
	int tm_sec;         /* seconds */
	int tm_min;         /* minutes */
	int tm_hour;        /* hours */
	int tm_mday;        /* day of the month */
	int tm_mon;         /* month */
	int tm_year;        /* year */
	int tm_wday;        /* day of the week */
	int tm_yday;        /* day in the year */
	int tm_isdst;       /* daylight saving time */
} __attribute__ ((packed)) tz_time_t;


typedef enum
{
	TZ_TIME_MSG_CMD_TIME_START		= 0x00000301,
	TZ_TIME_MSG_CMD_TIME_GET_UTC_SEC,
	TZ_TIME_MSG_CMD_TIME_GET_SYSTIME,
	TZ_TIME_MSG_CMD_TIME_GET_TIME_MS,
	TZ_TIME_MSG_CMD_TIME_GET_HLOS_UTC,
	TZ_TIME_MSG_CMD_TIME_END,
	TZ_TIME_MSG_CMD_UNKNOWN			= 0x7FFFFFFF
} tz_time_msg_cmd_type;

/* Command structure for getutcsec */
typedef struct tz_time_getutcsec_req_s
{
	/* First 4 bytes are always command id */
	tz_time_msg_cmd_type cmd_id;
} __attribute__ ((packed)) tz_time_getutcsec_req_t;

typedef struct tz_time_getutcsec_rsp_s
{
	/* First 4 bytes are always command id */
	tz_time_msg_cmd_type cmd_id;
	/* Pointer to timespec structure */
	tz_time_spec_t time_spec;
	/* Success/failure value */
	int ret;
} __attribute__ ((packed)) tz_time_getutcsec_rsp_t;


/* Command structure for systime */
typedef struct tz_time_getsystime_req_s
{
	/* First 4 bytes are always command id */
	tz_time_msg_cmd_type cmd_id;
} __attribute__ ((packed)) tz_time_getsystime_req_t;

typedef struct tz_time_getsystime_rsp_s
{
	/* First 4 bytes are always command id */
	tz_time_msg_cmd_type cmd_id;
	/* Pointer to time structure */
	tz_time_t time;
	/* Success/failure value */
	int ret;
} __attribute__ ((packed)) tz_time_getsystime_rsp_t;


/* Command structure for timems */
typedef struct tz_time_gettimems_req_s
{
	/* First 4 bytes are always command id */
	tz_time_msg_cmd_type cmd_id;
} __attribute__ ((packed)) tz_time_gettimems_req_t;

typedef struct tz_time_gettimems_rsp_s
{
	/* First 4 bytes are always command id */
	tz_time_msg_cmd_type cmd_id;
	/* Time in milliseconds */
	unsigned long ret;
} __attribute__ ((packed)) tz_time_gettimems_rsp_t;

/* Command structure for time end */
typedef struct tz_time_end_req_s
{
	/* First 4 bytes are always command id */
	tz_time_msg_cmd_type cmd_id;
} __attribute__ ((packed)) tz_time_end_req_t;

typedef struct tz_time_end_rsp_s
{
	/* First 4 bytes are always command id */
	tz_time_msg_cmd_type cmd_id;
	/* Success/failure value */
	int ret;
} __attribute__ ((packed)) tz_time_end_rsp_t;

typedef struct tz_time_err_rsp_s
{
	/* First 4 bytes are always command id */
	tz_time_msg_cmd_type cmd_id;
	/* Success/failure value */
	int ret;
} __attribute__ ((packed)) tz_time_err_rsp_t;

#endif //__TIME_MSG_H_
