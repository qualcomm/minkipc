// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

// The three symbols below are also exported by libminkadaptor, which carries a
// byte identical copy of this stub. Keeping this library's copy out of the
// dynamic symbol table means a process holding both libraries cannot end up
// resolving one library's supplicant against the other's receive path.
      .global    mink_recv_ioctl
      .global    mink_recv_skip
      .global    mink_recv_svc
      .hidden    mink_recv_ioctl
      .hidden    mink_recv_skip
      .hidden    mink_recv_svc
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
