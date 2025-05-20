/********************************************************************
Copyright (c) 2022 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
*********************************************************************/
#include <stdio.h>

static
int utoa_r(unsigned long in, char *buffer)
{
  unsigned long lim;
  int digits = 0;
  int pos = (~0UL > 0xfffffffful) ? 19 : 9;
  int dig;

  do {
    for (dig = 0, lim = 1; dig < pos; dig++) {
      lim *= 10;
    }

    if (digits || in >= lim || !pos) {
      for (dig = 0; in >= lim; dig++) {
        in -= lim;
      }
      buffer[digits++] = '0' + dig;
    }
  } while (pos--);

  buffer[digits] = 0;
  return digits;
}

static
int itoa_r(long in, char *buffer)
{
  char *ptr = buffer;
  int len = 0;

  if (in < 0) {
    in = -in;
    *(ptr++) = '-';
    len++;
  }
  len += utoa_r(in, ptr);
  return len;
}

int itoa(long in, char *buffer)
{
  if (NULL == buffer) {
    return 0;
  }

  return itoa_r(in, buffer);
}