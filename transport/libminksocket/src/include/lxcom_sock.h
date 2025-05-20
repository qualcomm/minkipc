/********************************************************************
 Copyright (c) 2016, 2023 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 *********************************************************************/
#ifndef __LXCOM_SOCK_H
#define __LXCOM_SOCK_H

#include <stdbool.h>
#include <sys/ioctl.h>
#include "object.h"

/*
 * Linux socket protocol
 */
#define LXCOM_MAX_ARGS 60
#define LXCOM_MSG_ALIGNMENT 8

/* Message types */
#define LXCOM_REQUEST       1
#define LXCOM_SUCCESS       2
#define LXCOM_ERROR         3
#define LXCOM_CLOSE         4

// Pre-allocate 32 bytes for LXCOM_CTL for extensibility
#define LXCOM_CTL_PREALLOC   32

// Invocation arguments for MinkSocket CTL
#define MINKSOCK_CTL_HANDLE   (-1)
#define MINKSOCK_CTL_OP_SYNC  1

static inline bool IsInvokeMsg(uint32_t m) {
  return (m == LXCOM_REQUEST ||
          m == LXCOM_SUCCESS ||
          m == LXCOM_ERROR);
}

typedef struct lxcom_hdr {
  uint32_t size;
  uint32_t type;
  uint32_t invoke_id;
} lxcom_hdr;

typedef union lxcom_arg {
  uint32_t size;
  struct {
    uint16_t flags;
    uint16_t handle;
  } o;
} lxcom_arg;

typedef struct lxcom_inv_req {
  lxcom_hdr hdr;
  int32_t handle;
  int32_t op;
  int32_t k;
  union lxcom_arg a[LXCOM_MAX_ARGS];
} lxcom_inv_req;

typedef struct lxcom_inv_succ {
  lxcom_hdr hdr;
  lxcom_arg a[LXCOM_MAX_ARGS];
} lxcom_inv_succ;

typedef struct lxcom_inv_err {
  lxcom_hdr hdr;
  int32_t  err;
} lxcom_inv_err;

typedef struct lxcom_close {
  uint32_t size;
  uint32_t type;
  uint32_t handle;
} lxcom_inv_close;

typedef union lxcom_ctl {
  uint32_t version;
  uint8_t placeholder[LXCOM_CTL_PREALLOC];
} lxcom_ctl;

typedef union {
  lxcom_hdr hdr;
  lxcom_inv_req req;
  lxcom_inv_succ succ;
  lxcom_inv_err err;
  lxcom_inv_close close;
} lxcom_msg;

#endif /* __LXCOM_IOCTL_H */
