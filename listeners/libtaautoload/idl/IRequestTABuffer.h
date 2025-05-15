// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// DO NOT EDIT (idlc v0.2.0)
#pragma once

#include <stdint.h>
#include "object.h"




#define IRequestTABuffer_OP_get 0

static inline int32_t
IRequestTABuffer_release(Object self)
{
    return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IRequestTABuffer_retain(Object self)
{
    return Object_invoke(self, Object_OP_retain, 0, 0);
}

/*
*
*   Return a Memory Object containing the TA binary
*
*   @param[in] UUID  UUID value identifying the TA.
*   @param[out] appElf Returns the TA Buffer as Object.
*
*   @return Object_OK on success.
*
*/
static inline int32_t IRequestTABuffer_get(Object self, const void *uuid_ptr, size_t uuid_len, Object *appElf)
{
    ObjectArg a[] = {
        {.bi = (ObjectBufIn) { uuid_ptr, uuid_len * sizeof(uint8_t) } },
        {.o = Object_NULL },
    };

    int32_t result = Object_invoke(self, IRequestTABuffer_OP_get, a, ObjectCounts_pack(1, 0, 0, 1));

    *appElf = a[1].o;
    return result;
}

