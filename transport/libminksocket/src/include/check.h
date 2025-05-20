/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/

#ifndef __CHECK_H
#define __CHECK_H

/**
 * @file check.h
 *
 * This file declares macros for dealing with errors.  Goals include:
 *
 *   - Making it easier to write code that checks errors correctly.
 *
 *   - Improving the readability of code.
 *
 *   - Encouraging a more systematic approach to error handling.
 *
 * Be advised that these macros embed control flow, so users should
 * take care to read and understand each of these.
 *
 * A convention encouraged here is that functions return integer error
 * codes, where 0 indicates success and non-zero values indicate failure.
 * The CHECK() and GUARD() encapsulate the following patterns, and add some
 * additional functionality that is discussed below.
 *
 *    CHECK(is_okay);    -->    if (! is_okay) {
 *                                 return 1;
 *                              }
 *
 *
 *    GUARD(status);     -->    if (status != 0) {
 *                                 return status;
 *                              }
 *
 * Example:
 *
 *    ...
 *    CHECK(a < max);
 *    CHECK(b < max);
 *    GUARD(computeValue(array[a], array[b]));
 *    ...
 *
 * Meaningful Errors and Diagnostics
 *
 * There is no need to define a distinct error code for every place in the
 * code where validation occurs.  While this can help the author with "in
 * the field" diagnostics, it pollutes the interface specification with
 * implementation details that are meaningless to the caller of the API.
 * Instead, to provide even better diagnostics and to avoid such pollution,
 * we log each CHECK failure.
 *
 * CHECK() should be used for exceptional conditions: those that are not
 * encountered when all software in the system is functioning properly. For
 * example, most bounds-checking falls into this category.
 *
 * GUARD() does not log additional information.  It re-reports a previously-
 * detected error condition.  An analogy can be made to exception handling
 * in languages like C++ and Java: a CHECK failure "throws", and GUARD will
 * "re-throw".
 *
 * CHECK_LOG() logs the file and line at which CHECK was called.  In unit
 * test builds, these are printed to stdout.
 *
 * Cleanup
 *
 * Some functions allocate and free resources during a call.  In these
 * cases, a "return" in between the allocation and the free could lead to a
 * leak.  One of the following approaches are recommended:
 *
 *  1. Store the pointer to the resource in an object (as a "member
 *     variable").  This allows the resource to be freed later when the
 *     destructor ("deinit" or "free" function) is called.
 *
 *  2. Ensure that no CHECK, GUARD, or return statements occur between the
 *     allocation and the free.  For example, the code could copy out values
 *     that can then be validated after the free.
 *
 *  3. Use the "goto bail" convention.
 *
 *     Free all resources in only one place, at the bottom of the function,
 *     following a label named "bail".  At the top of the function,
 *     intialize variables that hold resource pointers to NULL.  Within the
 *     body of the function, use "goto bail" whenever an error is
 *     encountered.  Since both success and failure cases will fall through
 *     to "bail", initialize the return code to "failure".
 *
 *     Instead of CHECK() use CHECK_ELSE(..., goto bail).
 *
 *     Instead of GUARD() use `if (status) goto bail;`.
 *
 */

//#include "heap_port.h"

#if defined(_DEBUG) || defined(OFFTARGET)

#include <stdio.h>

static int check__quiet __attribute__((unused)) = 0;

static inline void check_log(const char *file, int line) {
  printf("%s:%d: unexpected error\n", file, line);
}

#define CHECK__LOG()  check_log(__FILE__, __LINE__)

/*
 * Log current file and line indicating exceptional condition.
 */
#define CHECK_LOG()  (check__quiet ? (void) 0 : (void) CHECK__LOG())

/*
 * CHECK_QUIET(is_quiet) { statements; }
 *
 * Execute `statements` with check logging suppressed (is_quiet=1) or
 * enabled (is_quiet=0).
 */
#define CHECK_QUIET(is_quiet)                            \
  for (int _n=1, _o=check__quiet;                        \
       _n != 0 && (check__quiet = (is_quiet), 1);        \
       _n=0, check__quiet=_o)

#else /* _DEBUG */

#define CHECK__LOG()
#define CHECK_LOG()  CHECK__LOG()
#define CHECK_QUIET(b)  for (int _t=1; _t; _t=0)

#endif /* _DEBUG */


#define CHECK_E(is_valid, code)         \
  do {                                  \
    if (!(is_valid)) {                  \
      CHECK_LOG();                      \
      return (code);                    \
    };                                  \
  } while (0)

#define CHECK(is_valid)                 \
  do {                                  \
    if (!(is_valid)) {                  \
      CHECK_LOG();                      \
      return 1;                         \
    };                                  \
  } while (0)

#define CHECK_ELSE(is_valid, else_do)   \
  do {                                  \
    if (!(is_valid)) {                  \
      CHECK_LOG();                      \
      else_do;                          \
    };                                  \
  } while (0)

#define GUARD(status)                   \
  do {                                  \
    int _st = (status);                 \
    if (_st != 0) return _st;           \
  } while (0)

#define ERR_CLEAN(e) \
  do {                                         \
    if (!Object_isOK(err = (e))) {             \
        goto cleanup; }                        \
  } while(0)

#define CHECK_CLEAN(expr) \
  do {                                           \
    if (!(expr)) { CHECK_LOG(); goto cleanup; }  \
  } while (0)

#endif // __CHECK_H
