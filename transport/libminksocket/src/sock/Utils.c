/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#ifdef __ANDROID__
#include <cutils/properties.h>
#endif /* __ANDROID__ */
#include "Utils.h"

static bool gIsTraceEnable = false;

void Utils_configTrace(void)
{
#ifdef OFFTARGET
    char* isTraceEnable = getenv("minksocketFullLog");
    if (NULL != isTraceEnable) {
        gIsTraceEnable = true;
    }
#else
#ifdef __ANDROID__
#ifdef MINK_TRACE_ON
    char traceEnableStr[PROPERTY_VALUE_MAX];
    if (property_get("vendor.minksocketFullLog", traceEnableStr, NULL)) {
        gIsTraceEnable = true;
    }
#endif /* MINK_TRACE_ON */
#else
    if (access("/tmp/minksocketFullLog", F_OK) == 0) {
        gIsTraceEnable = true;
    }
#endif /* __ANDROID__ */
#endif /* OFFTARGET */
}

bool Utils_getTrace(void)
{
    return gIsTraceEnable;
}
