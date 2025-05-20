/* Copyright (c) 2011-2013, 2016, 2021 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

// qtest supplies utilities for unit testing.

#ifndef __QTEST_H
#define __QTEST_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>


#define qt_fail(xfmt, ...)                                              \
  (printf("%s:%u: " xfmt, __FILE__, __LINE__, __VA_ARGS__), exit(1))


#ifdef QTEST_VERBOSE
#  define qt__showab(a, b, fmt)                                 \
  printf("%s:%u:\ntest A: " fmt "\n     B: " fmt "\n", __func__, __LINE__, (a), (b))
#else
#  define qt__showab(a, b, fmt)  0
#endif


#define qt__assert_eq(aa, bb, isEQ, ty, fmt)                      \
  do {                                                          \
    ty _a = (ty) (aa);                                           \
    ty _b = (ty) (bb);                                           \
    (void) qt__showab(_a, _b, fmt);                              \
    if (!isEQ(_a, _b)) {                                        \
      qt_fail("values not equal:\n"                             \
               "A: " fmt "\n"                                   \
               "B: " fmt "\n", _a, _b);                         \
    }                                                           \
  } while (0)


#define qt__isEQ(a, b)  ((a) == (b))


#define qt_assert(x)                                    \
  ((x) ? 1 : (qt_fail("assertion failed: %s\n", #x), 0))


#define qt_eqb(a, b)   qt__assert_eq(a, b, qt__isEQ, bool, "%d")

#define qt_eqi(a, b)   qt__assert_eq(a, b, qt__isEQ, intmax_t, "%" PRIdMAX)

#define qt_equ(a, b)   qt__assert_eq(a, b, qt__isEQ, uintmax_t, "%" PRIuMAX)

#define qt_eqx(a, b)   qt__assert_eq(a, b, qt__isEQ, uintmax_t, "%" PRIxMAX )

#define qt_eqp(a, b)   qt__assert_eq(a, b, qt__isEQ, const void *, "%p")

#define qt_eqfp(a, b)  qt__assert_eq((intptr_t)a, (intptr_t)b, qt__isEQ, intptr_t, "%" PRIdPTR)

#define qt_eqs(a, b)   qt__assert_eq(a, b, !strcmp, const char *, "\"%s\"")


#endif /* __QTEST_H */
