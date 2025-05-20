// Copyright (c) 2015 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.

// kdefs: definitions for the kernel environment

#ifndef __KDEFS_H
#define __KDEFS_H

#include <stdlib.h>
#include <stdio.h>

#ifdef _DEBUG
#  include "qtest.h"
#  define kassert(x)       qt_assert(x)
#  define kassert_eqi(a,b) qt_eqi(a,b)
#  define kassert_equ(a,b) qt_equ(a,b)
#  define kassert_eqb(a,b) qt_eqb(a,b)
#  define kassert_eqp(a,b) qt_eqp(a,b)
#  define kassert_eqs(a,b) qt_eqs(a,b)
#else
#  define kassert(x)       ((void)0)
#  define kassert_eqi(a,b) ((void)0)
#  define kassert_equ(a,b) ((void)0)
#  define kassert_eqb(a,b) ((void)0)
#  define kassert_eqp(a,b) ((void)0)
#  define kassert_eqs(a,b) ((void)0)
#endif

#ifdef VERBOSE
# define KLOG(...)  printf(__VA_ARGS__)
#else
# define KLOG(...)  ((void)0)
#endif

#endif /* __KDEFS_H */
