/********************************************************************
 Copyright (c) 2024 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 *********************************************************************/
#ifndef __CREDENTIALSADAPTER_H
#define __CREDENTIALSADAPTER_H

#include "object.h"

#if defined (__cplusplus)
extern "C" {
#endif

int32_t LocalCredAdapter_new(Object endpoint, Object credentials, Object *objOut);

int32_t RemoteCredAdapter_new(Object endpoint, Object credentials, Object *objOut);

#if defined (__cplusplus)
}
#endif

#endif //__CREDENTIALSADAPTER_H
