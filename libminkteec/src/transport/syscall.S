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
