/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/

#include "Profiling.h"
#include <unistd.h>

#ifdef __ANDROID__
#include <cutils/properties.h>
#endif /* __ANDROID__ */

static bool gIsProfileEnable = false;

void Profiling_configProfile(void)
{
#ifdef OFFTARGET
    char* isProfileEnable = getenv("minksocketProfileLog");
    if (NULL != isProfileEnable) {
        gIsProfileEnable = true;
    }
#else
#ifdef __ANDROID__
    char profileEnableStr[PROPERTY_VALUE_MAX];
    if (property_get("vendor.minksocketProfileLog", profileEnableStr, NULL)) {
        gIsProfileEnable = true;
    }
#else
    if (access("/tmp/minksocketProfileLog", F_OK) == 0) {
        gIsProfileEnable = true;
    }
#endif /* __ANDROID__ */
#endif /* OFFTARGET */
}

bool Profiling_getProfile(void)
{
    return gIsProfileEnable;
}
