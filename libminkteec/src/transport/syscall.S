// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

      .global    mink_recv_ioctl
      .global    mink_recv_skip
      .global    mink_recv_svc
      .p2align   4
mink_recv_ioctl:
      ldr     w4, [x3]
      cbnz    w4, mink_recv_skip
      mov     x8, 29 // ioctl syscall is identified by 29 on ARM
mink_recv_svc:
      svc     0
      ret
mink_recv_skip:
      mov     x0, -4 // -EINTR
      ret

      .end
