// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

      .global    mink_recv_ioctl
      .global    mink_recv_skip
      .global    mink_recv_svc
      .p2align   4

mink_recv_ioctl:
      ldr     r4, [r3]
      cmp     r4, #0
      bne     mink_recv_skip
      mov     r7, #29
mink_recv_svc:
      svc     #0
      bx      lr
mink_recv_skip:
      mov     r0, #-4
      bx      lr

      .end
