/********************************************************************
 Copyright (c) 2022-2023 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
*********************************************************************/

/** @file  CSampleService_open.h */

#ifndef __CSAMPLESERVICE_OPEN_H
#define __CSAMPLESERVICE_OPEN_H

#include <stdint.h>
#include "object.h"

int32_t CSampleService_open(uint32_t uid, Object credentials, Object *objOut);

#endif  // __CSAMPLESERVICE_OPEN_H
