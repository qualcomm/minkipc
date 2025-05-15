// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <stdint.h>
#include "object.h"

#define IRegisterListenerCBO_ERROR_MAX_REGISTERED INT32_C(10)
#define IRegisterListenerCBO_ERROR_ALIGNMENT INT32_C(11)
#define IRegisterListenerCBO_ERROR_ID_IN_USE INT32_C(12)
#define IRegisterListenerCBO_ERROR_ID_RESERVED INT32_C(13)
#define IRegisterListenerCBO_ERROR_REG_NOT_ALLOWED_FROM_CURRENT_VM INT32_C(14)

#define IRegisterListenerCBO_OP_register 0

static inline int32_t
IRegisterListenerCBO_release(Object self)
{
    return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IRegisterListenerCBO_retain(Object self)
{
    return Object_invoke(self, Object_OP_retain, 0, 0);
}

/*
*
*   Register a CBO-style listener with QTEE.
*
*   Each CBO listener can be associated with a shared memory object.
*   Each CBO implements \link IListenerCBO \endlink .
*
*   @param[in] listenerId  The listener id being registered.
*   @param[in] cbo         The callback object associated with this listener.
*   @param[in] memRegion   The shared memory object associated with this listener.
*                          Can be Object_NULL.
*
*   @return
*   Object_OK on success.
*
*/
static inline int32_t IRegisterListenerCBO_register(Object self, uint32_t listenerId_val, Object cbo, Object memRegion)
{
    ObjectArg a[] = {
        {.b = (ObjectBuf) { &listenerId_val, sizeof(uint32_t) } },
        {.o = cbo },
        {.o = memRegion },
    };

    int32_t result = Object_invoke(self, IRegisterListenerCBO_OP_register, a, ObjectCounts_pack(1, 0, 2, 0));

    return result;
}



