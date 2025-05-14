// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <stdint.h>
#include "object.h"

#define IListenerCBO_OP_request 0
#define IListenerCBO_OP_wait 1

static inline int32_t
IListenerCBO_release(Object self)
{
    return Object_invoke(self, Object_OP_release, 0, 0);
}

static inline int32_t
IListenerCBO_retain(Object self)
{
    return Object_invoke(self, Object_OP_retain, 0, 0);
}

/*
*
*  Used to invoke the callback object of a CBO-style listener,
*  instructing it to inspect its associated shared memory object
*  and perform the specified service.
*
*  When used with a QComCompat TA, the listener can include
*  embedded pointers in its response, which are represented
*  by the accompanying shared memory objects.
*
*  @param[out]  embeddedBufOffsets  An array containing offsets into the request buffer
*                                   at which address information for each memory object
*                                   is to be written.
*  @param[out]  is64     Flag describing if the addresses are to be treated as 64-bit
*                        addressable or 32-bit addressable
*  @param[out]  smo1     Shared memory object 1
*  @param[out]  smo2     Shared memory object 2
*  @param[out]  smo3     Shared memory object 3
*  @param[out]  smo4     Shared memory object 4
*
*
*  @return
*  Object_OK on success.
*
*/
static inline int32_t IListenerCBO_request(Object self, uint32_t *embeddedBufOffsets_ptr, size_t embeddedBufOffsets_len, size_t *embeddedBufOffsets_lenout, uint32_t *is64_ptr, Object *smo1, Object *smo2, Object *smo3, Object *smo4)
{
    ObjectArg a[] = {
        {.b = (ObjectBuf) { embeddedBufOffsets_ptr, embeddedBufOffsets_len * sizeof(uint32_t) } },
        {.b = (ObjectBuf) { is64_ptr, sizeof(uint32_t) } },
        {.o = Object_NULL },
        {.o = Object_NULL },
        {.o = Object_NULL },
        {.o = Object_NULL },
    };

    int32_t result = Object_invoke(self, IListenerCBO_OP_request, a, ObjectCounts_pack(0, 2, 0, 4));
    *embeddedBufOffsets_lenout = a[0].b.size / sizeof(uint32_t);
    *smo1 = a[2].o;
    *smo2 = a[3].o;
    *smo3 = a[4].o;
    *smo4 = a[5].o;

    return result;
}

/*
*
*   When a TA/QTEE is already accessing a listener/ CBO, the current
*   TA/QTEE will wait till the previous TA/QTEE is done accessing the
*  listener. This function will help the current TA/QTEE to wait in the
*  HLOS till the previous TA/QTEE is done accesing the listener/ CBO.
*
*   @param[in] None
*
*   @return
*   Object_OK on success.
*
*/
static inline int32_t IListenerCBO_wait(Object self)
{
    return Object_invoke(self, IListenerCBO_OP_wait, 0, 0);;
}


