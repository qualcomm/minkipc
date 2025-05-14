// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "time_msg.h"

int smci_dispatch(void *dma_buf, size_t dma_buf_len);

/**
 * @brief Returns seconds in UTC as a tz_time_spec.
 *
 * @param time_spec Structure to hold UTC Time.
 * @return Returns result of getting the time.
 */
static int get_utc_seconds(tz_time_spec_t *time_spec)
{
	int ret = 0;
	struct timespec my_time_spec = {0, 0};

	MSGD("QSEE Time Listener: get_utc_seconds\n");

	ret = clock_gettime(CLOCK_REALTIME, &my_time_spec);
	time_spec->tv_sec = (uint32_t)my_time_spec.tv_sec;
	time_spec->tv_nsec = my_time_spec.tv_nsec;

	MSGD("QSEE Time Listener: seconds: %d\n", time_spec->tv_sec);
	MSGD("QSEE Time Listener: nano seconds: %d\n", time_spec->tv_nsec);

	return ret;
}

/**
 * @brief Returns system time in tz_time_t spec.
 *
 * @param time Time Spec to return result in.
 * @return Returns result of clock operation.
 */
static int get_systime(tz_time_t *time)
{
	int ret = 0;
	time_t utc_sec;
	struct timespec my_time_spec = {0, 0};
	struct tm *my_time = NULL;

	MSGD("QSEE Time Listener: get_systime\n");

	do {
		my_time = (struct tm *)malloc(sizeof(struct tm));
		if(!my_time) {
			ret = -1;
			MSGE("ERROR: malloc failed\n");
			break;
		}

		if(clock_gettime(CLOCK_REALTIME, &my_time_spec)) {
			ret = -1;
			MSGE("ERROR: clock_gettime failed\n");
			break;
		}

		utc_sec = my_time_spec.tv_sec;
		if(gmtime_r((const time_t *) &utc_sec, my_time) == NULL) {
			ret = -1;
			MSGE("ERROR: gmtime_r failed\n");
			break;
		}

		(*time).tm_sec = (*my_time).tm_sec;
		(*time).tm_min = (*my_time).tm_min;
		(*time).tm_hour = (*my_time).tm_hour;
		(*time).tm_mday = (*my_time).tm_mday;
		(*time).tm_mon = (*my_time).tm_mon;
		(*time).tm_year = (*my_time).tm_year;
		(*time).tm_wday = (*my_time).tm_wday;
		(*time).tm_yday = (*my_time).tm_yday;
		(*time).tm_isdst = (*my_time).tm_isdst;

	} while(0);

	if(my_time)
		free(my_time);

	return ret;
}

/**
 * @brief Returns time in milliseconds as return value.
 *
 * @return Time in milliseconds.
 */
static unsigned long get_time_ms(void)
{
	unsigned long ms = 0;
	time_t sec = 0;
	long nsec = 0;
	struct timespec my_time_spec = {0, 0};

	MSGD("QSEE Time Listener: get_time_ms\n");

	if(clock_gettime(CLOCK_REALTIME, &my_time_spec)) {
		MSGE("ERROR: clock_gettime failed\n");
		return -1;
	}

	sec = my_time_spec.tv_sec;
	nsec = my_time_spec.tv_nsec;

	ms = sec * 1000;
	if(nsec > 1000000)
		ms += nsec/1000000;

	return ms;
}

/**
 * @brief Returns seconds in UTC.
 *
 * @param req Request structure as provided by the caller QTEE application.
 * @param rsp Response structure as provided by the caller QTEE application.
 * @return Always returns 0 as in success.
 */
static int time_getutcsec(void *req, void *rsp)
{
	tz_time_getutcsec_rsp_t *my_rsp = (tz_time_getutcsec_rsp_t *)rsp;
	tz_time_spec_t time_spec = {0,0};

	UNUSED(req);
	MSGD("QSEE Time Listener: time_getutcsec\n");

	my_rsp->ret = get_utc_seconds(&time_spec);

	my_rsp->time_spec.tv_sec = time_spec.tv_sec;
	my_rsp->time_spec.tv_nsec = time_spec.tv_nsec;

	MSGD("time_getutcsec returns %d, sec = %d; nsec = %d\n", my_rsp->ret,
	     my_rsp->time_spec.tv_sec, my_rsp->time_spec.tv_nsec);

	return 0;
}

/**
 * @brief Returns seconds in UTC.
 *
 * @param req Request structure as provided by the caller QTEE application.
 * @param rsp Response structure as provided by the caller QTEE application.
 * @return Always returns 0 as in success.
 */
static int time_gethlosutc(void *req, void *rsp)
{
	tz_time_getutcsec_rsp_t *my_rsp = (tz_time_getutcsec_rsp_t *)rsp;
	tz_time_spec_t time_spec = {0,0};
	UNUSED(req);

	MSGD("QSEE Time Listener: time_gethlosutc\n");

	my_rsp->ret = get_utc_seconds(&time_spec);

	my_rsp->time_spec.tv_sec = time_spec.tv_sec;
	my_rsp->time_spec.tv_nsec = time_spec.tv_nsec;

	MSGD("time_gethlosutc returns %d, sec = %d; nsec = %d\n", my_rsp->ret,
	     my_rsp->time_spec.tv_sec, my_rsp->time_spec.tv_nsec);

	return 0;
}

/**
 * @brief Retrieves the system time.
 *
 * @param req Request structure as provided by the caller QTEE application.
 * @param rsp Response structure as provided by the caller QTEE application.
 * @return Returns result of time operation.
 */
static int time_getsystime(void *req, void *rsp)
{
	tz_time_getsystime_rsp_t *my_rsp = (tz_time_getsystime_rsp_t *)rsp;
	tz_time_t time = {0,0,0,0,0,0,0,0,0};
	UNUSED(req);

	MSGD("QSEE Time Listener: time_getsystime\n");

	my_rsp->ret = get_systime(&time);
	memcpy(&(my_rsp->time), &time, sizeof(tz_time_t));
	MSGD("time_getsystime tm_sec %d\n", my_rsp->time.tm_sec);
	MSGD("time_getsystime tm_min %d\n", my_rsp->time.tm_min);
	MSGD("time_getsystime tm_mday %d\n", my_rsp->time.tm_mday);
	MSGD("time_getsystime tm_mon %d\n", my_rsp->time.tm_mon);
	MSGD("time_getsystime tm_year %d\n", my_rsp->time.tm_year);
	MSGD("time_getsystime tm_wday %d\n", my_rsp->time.tm_wday);
	MSGD("time_getsystime tm_yday %d\n", my_rsp->time.tm_yday);
	MSGD("time_getsystime tm_isdst %d\n", my_rsp->time.tm_isdst);
	MSGD("time_getsystime returns %d\n", my_rsp->ret);

	return 0;
}

/**
 * @brief Retrieves the time in milliseconds.
 *
 * @param req Request structure as provided by the caller QTEE application.
 * @param rsp Response structure as provided by the caller QTEE application.
 * @return Returns the result of the time operation.
 */
static unsigned long time_gettimems(void *req, void *rsp)
{
	tz_time_gettimems_rsp_t *my_rsp = (tz_time_gettimems_rsp_t *)rsp;
	UNUSED(req);

	MSGD("QSEE Time Listener: time_gettimems\n");

	my_rsp->ret = get_time_ms();
	MSGD("time_gettimems return %ld ms\n", my_rsp->ret);

	return 0;
}

/**
 * @brief Notifies the listener that the time service should end it's operation.
 *
 * @param req Request structure as provided by the caller QTEE application.
 * @param rsp Response structure as provided by the caller QTEE application.
 * @return Always returns 0 as in success.
 */
static int time_end(void *req, void *rsp)
{
	tz_time_end_rsp_t *my_rsp = (tz_time_end_rsp_t *)rsp;
	UNUSED(req);

	MSGD("QSEE Time Listener: time_end\n");

	(*my_rsp).ret = 0;

	return 0;
}

/**
 * @brief Handles time errors.
 *
 * @param req Request structure as provided by the caller QTEE application.
 * @return Always returns 0 as in success.
 */
static int time_error(void *rsp)
{
	tz_time_err_rsp_t *my_rsp = (tz_time_err_rsp_t *)rsp;

	MSGD("QSEE Time Listener: time_error\n");

	(*my_rsp).ret = -1;

	return 0;
}

int smci_dispatch(void *dma_buf, size_t dma_buf_len)
{
	int ret = -1;
	tz_time_msg_cmd_type time_cmd_id;

	MSGD("Time dispatch starts\n");

	/* Buffer size is 4K page-aligned and should always be big enough to
	 * accomodate the largest of time structs.
	 */
	if (dma_buf_len < TZ_MAX_BUF_LEN) {
		MSGE("[%s:%d] Invalid buffer len.\n", __func__, __LINE__);
		return -1; // This should be reported as a transport error
	}

	time_cmd_id = *((tz_time_msg_cmd_type *)dma_buf);
	MSGD("time_cmd_id = 0x%x\n", time_cmd_id);

	switch(time_cmd_id)
	{
	  case TZ_TIME_MSG_CMD_TIME_GET_UTC_SEC:
		MSGD("time_getutcsec starts!\n");
		ret = time_getutcsec(dma_buf, dma_buf);
		MSGD("time_getutcsec finished!\n");
		break;
	  case TZ_TIME_MSG_CMD_TIME_GET_HLOS_UTC:
		MSGD("time_gethlosutc starts!\n");
		ret = time_gethlosutc(dma_buf, dma_buf);
		MSGD("time_gethlosutc finished!\n");
		break;
	  case TZ_TIME_MSG_CMD_TIME_GET_SYSTIME:
		MSGD("time_getsystime starts!\n");
		ret = time_getsystime(dma_buf, dma_buf);
		MSGD("time_getsystime finished!\n");
		break;
	  case TZ_TIME_MSG_CMD_TIME_GET_TIME_MS:
		MSGD("time_gettimems starts!\n");
		ret = time_gettimems(dma_buf, dma_buf);
		MSGD("time_gettimems finished!\n");
		break;
	  case TZ_TIME_MSG_CMD_TIME_END:
		MSGD("time_services Dispatch end request!\n");
		ret = time_end(dma_buf, dma_buf);
		break;
	  default:
		MSGD("ERROR: command %d is not found!\n", time_cmd_id);
		ret = time_error(dma_buf);
		break;
	}

	MSGD("time_services Dispatch ends and ret = %d!\n", ret);
	return ret;
}
