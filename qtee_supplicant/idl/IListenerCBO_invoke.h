// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once

#include <stdint.h>
#include "object.h"
#include "IListenerCBO.h"

#ifdef __clang__
#define __compiler_pragma_pre \
    _Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wignored-attributes\"")
#define __compiler_pragma_post _Pragma("clang diagnostic pop")
#else
#define __compiler_pragma_pre
#define __compiler_pragma_post
#endif

#define IListenerCBO_DEFINE_INVOKE(func, prefix, type) \
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
            case IListenerCBO_OP_request: { \
                if (k != ObjectCounts_pack(0,2,0,4) || \
                    a[1].b.size != 4) { \
                    break; \
                } \
                 \
                uint32_t *embeddedBufOffsets_ptr = (uint32_t*)a[0].b.ptr; \
                size_t embeddedBufOffsets_len = a[0].b.size / 4; \
                uint32_t *is64_ptr = (uint32_t*)a[1].b.ptr; \
                Object *smo1 = &a[2].o; \
                Object *smo2 = &a[3].o; \
                Object *smo3 = &a[4].o; \
                Object *smo4 = &a[5].o; \
                int32_t r = prefix##request(me, embeddedBufOffsets_ptr, embeddedBufOffsets_len, &embeddedBufOffsets_len, is64_ptr, smo1, smo2, smo3, smo4); \
                 \
                a[0].b.size = embeddedBufOffsets_len * 4; \
                return r; \
            }  \
            case IListenerCBO_OP_wait: { \
                if (k != ObjectCounts_pack(0,0,0,0)) { \
                    break; \
                } \
                 \
                int32_t r = prefix##wait(me); \
                 \
                return r; \
            }  \
        } \
        return Object_ERROR_INVALID; \
    }
