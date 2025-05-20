/********************************************************************
 Copyright (c) 2022 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 *********************************************************************/
#ifndef __XTZDCREDENTIALS_H
#define __XTZDCREDENTIALS_H

typedef struct XtzdCredentials XtzdCredentials;

#if defined (__cplusplus)
extern "C" {
#endif

int32_t XtzdCredentials_newFromCBO(Object linkCred, Object *objOut);

#if defined (__cplusplus)
}
#endif

#endif //__XTZDCREDENTIALS_H
