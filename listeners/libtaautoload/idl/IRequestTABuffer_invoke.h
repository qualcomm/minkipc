// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// DO NOT EDIT (idlc v0.2.0)
#pragma once

#include <stdint.h>
#include "object.h"
#include "IRequestTABuffer.h"

#ifdef __clang__
#define __compiler_pragma_pre \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wignored-attributes\"")
#define __compiler_pragma_post _Pragma("clang diagnostic pop")
#else
#define __compiler_pragma_pre
#define __compiler_pragma_post
#endif

#define IRequestTABuffer_DEFINE_INVOKE(func, prefix, type) \
    int32_t func(ObjectCxt h, ObjectOp op, ObjectArg *a, ObjectCounts k) \
    { \
        __compiler_pragma_pre \
         \
        __compiler_pragma_post \
        type me = (type) h; \
        switch (ObjectOp_methodID(op)) { \
            case Object_OP_release: { \
                if (k != ObjectCounts_pack(0, 0, 0, 0)) { \
                    break; \
                } \
                return prefix##release(me); \
            } \
            case Object_OP_retain: { \
                if (k != ObjectCounts_pack(0, 0, 0, 0)) { \
                    break; \
                } \
                return prefix##retain(me); \
            } \
             \
            case IRequestTABuffer_OP_get: { \
                if (k != ObjectCounts_pack(1,0,0,1)) { \
                    break; \
                } \
                 \
                const void *uuid_ptr = (const void*)a[0].b.ptr; \
                size_t uuid_len = a[0].b.size; \
                Object *appElf = &a[1].o; \
                int32_t r = prefix##get(me, uuid_ptr, uuid_len, appElf); \
                 \
                return r; \
            }  \
        } \
        return Object_ERROR_INVALID; \
    }
