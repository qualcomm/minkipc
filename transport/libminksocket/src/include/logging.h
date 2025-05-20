/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/

#ifndef __LOGGING_H
#define __LOGGING_H

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "minksocket"
#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#define LOGP LOG
#else
#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#define LOG_MSG_LEN 2048
static char __LogBuffer[LOG_MSG_LEN];
static pthread_mutex_t logPrintfMtx __attribute__((unused)) = PTHREAD_MUTEX_INITIALIZER;

static inline void _log_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(__LogBuffer, sizeof(__LogBuffer), fmt, args);
    va_end(args);
    write(STDOUT_FILENO, __LogBuffer, strlen(__LogBuffer));
}

#define LOG_PRINTF(...)                                                                          \
    if (!pthread_mutex_lock(&logPrintfMtx)) {                                                    \
        _log_printf(__VA_ARGS__);                                                                \
        pthread_mutex_unlock(&logPrintfMtx);                                                     \
    } else {                                                                                     \
        printf("Info  %s:%u logging.h PRINTF Macro skipped due to mutex lock error\n",           \
               __func__, __LINE__);                                                              \
    }

#define LOG LOG_PRINTF
#define LOGE LOG
#define LOGP(...) { printf("MinkSocket: " __VA_ARGS__); fflush(stdout); }
#endif /* __ANDROID__ */

#define FATAL(...) { LOGE(__VA_ARGS__); exit(-1); }
#define LOGF() LOG("%s:%d\n", __FUNCTION__, __LINE__)

#define ALOGE LOGE
#define ALOG LOG
#define ALOGD LOG
#define ALOGV LOG
#endif /* __LOGGING_H */


