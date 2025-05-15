// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// DO NOT EDIT (idlc v0.2.0)
#pragma once

#include <stdint.h>
#include "object.h"
#include "IRequestTABuffer.h"




#define IRegisterTABufCBO_OP_register 0

static inline int32_t
IRegisterTABufCBO_release(Object self)
{
    return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IRegisterTABufCBO_retain(Object self)
{
    return Object_invoke(self, Object_OP_retain, 0, 0);
}

/*
*
*   Register a CBO used by HLOS GP Stack to return a memory object filled with
*   signed TA elf identified by passed input UUID.
*
*   @param[in] CBO to be registered by QTEE for TA auto loading.
*
*   @return
*   Object_OK on success.
*
*/
static inline int32_t IRegisterTABufCBO_register(Object self, Object obj)
{
    ObjectArg a[] = {
        {.o = obj },
    };

    int32_t result = Object_invoke(self, IRegisterTABufCBO_OP_register, a, ObjectCounts_pack(0, 0, 1, 0));

    return result;
}

