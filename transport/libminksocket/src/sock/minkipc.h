/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#ifndef __MINKIPC_H
#define __MINKIPC_H

#include "VmOsal.h"
#include "object.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define OPENER 0
#define MODULE 1

typedef struct MinkIPC MinkIPC;

/*@brief: actual implementation to start Mink service for kinds of scenarios
 *
 *@param[in] address: string of socket file name where service works
             Only apply to ipcType of UNIX or SIMULATED. New socket file
             descriptor will be constructed internally according to it.
 *@param[in] sockFd: constructed socket file descriptor which service works on
 *           Argument of 'address' and 'sockFd' are exlusive
 *@param[in] ipcType: protocol type based on which service works
             Only if service plays the role of hub/xtzd, it can work on ipcType
             of SIMULATED, which simulates non-UNIX interaction with UNIX one
 *@param[in] endpoint: the Mink object to be exposed as service
 *@param[in] endpointType: interface type of exposed service
 *           'OPENER' implements IOpener interface. 'MODULE' implements IModule interface
 *           If the service plays the role of hub/xtzd, it must be 'MODULE' type
 **@param[out] instanceOut: MinkIPC instance pointing to established Mink interaction
 *
 *@return: succeed: Object_OK. fail: other positive or negative value;
*/
int32_t MinkIPC_beginService(const char *address, int32_t sockFd, int32_t ipcType,
                             Object endpoint, int32_t endpointType,
                             MinkIPC** instanceOut);

/*@brief: start a Mink serive to expose functionality of given object.
 *        specified for UNIX protocol and IOpener based object
 *
 *@param[in] address: string of socket file name where service works
 *           Only apply to ipcType of UNIX or SIMULATED. New socket file
 *           descriptor will be constructed internally according to it.
 *@param[in] endpoint: the Mink object to be exposed as service
*/
MinkIPC *MinkIPC_startService(const char *address, Object endpoint);

/*@brief: start a Mink serive to expose functionality of given object.
 *        specified for UNIX protocol and IOpener based object
 *
 *@param[in] sock: constructed socket file descriptor which service works on
 *@param[in] endpoint: the Mink object to be exposed as service
*/
MinkIPC *MinkIPC_startServiceOnSocket(int32_t sock, Object endpoint);

/*@brief: start a Mink serive to expose functionality of given object.
 *        specified for UNIX protocol and IModule based object
 *
 *@param[in] the same to annotation above
*/
MinkIPC *MinkIPC_startServiceModule(const char *address, Object endpoint);

/*@brief: start a Mink serive to expose functionality of given object.
 *        specified for UNIX protocol and IModule based object
 *
 *@param[in] the same to annotation above
*/
MinkIPC *MinkIPC_startServiceModuleOnSocket(int32_t sock, Object endpoint);

/*@brief: start a Mink serive to expose functionality of given object.
 *        specified for SIMULATED scenario and IModule based object
 *        The interface only applies to object plays the role of hub/xtzd
 *
 *@param[in] the same to annotation above
*/
MinkIPC *MinkIPC_startServiceModule_simulated(const char *address, Object endpoint);

/*@brief: start a Mink serive to expose functionality of given object.
 *        specified for SIMULATED scenario and IModule based object
 *        The interface only applies to object plays the role of hub/xtzd
 *
 *@param[in] the same to annotation above
*/
MinkIPC *MinkIPC_startServiceModuleOnSocket_simulated(int32_t sock, Object endpoint);

/*@brief: connect target Mink Service then interact with output proxy
 *
 *@param[in] the same to annotation above
*/
MinkIPC* MinkIPC_connect_common(const char *address, Object *proxyOut, int32_t ipcType);

/*@brief: connect target Mink Service then interact with output proxy.
 *        specified for UNIX protocol and IOpener based object
 *        IOpener or IModule interface make no difference during connection.
 *        Discriminate them just to match function list of MinkIPC_startServiceXXX()
 *
 *@param[in] the same to annotation above
*/
MinkIPC *MinkIPC_connect(const char *address, Object *proxyOut);

/*@brief: connect target Mink Service then interact with output proxy.
 *        specified for simulated and IOpener based object
 *
 *@param[in] the same to annotation above
*/
MinkIPC *MinkIPC_connect_simulated(const char *address, Object *proxyOut);

/*@brief: connect target Mink Service then interact with output proxy.
 *        specified for UNIX protocol and IModule based object
 *
 *@param[in] the same to annotation above
*/
MinkIPC *MinkIPC_connectModule(const char *address, Object *proxyOut);

/*@brief: connect target Mink Service then interact with output proxy.
 *        specified for simulated and IModule based object
 *
 *@param[in] the same to annotation above
*/
MinkIPC *MinkIPC_connectModule_simulated(const char *address, Object *proxyOut);

/**
   wait for the service to finish ..
   waits until stopped or the service dies
**/
void MinkIPC_join(MinkIPC *me);

/**
   Increment reference count to keep the object live.
**/
void MinkIPC_retain(MinkIPC *me);

/**
   Decrement reference count.
   When the count goes to 0, *me* is deleted.
**/
void MinkIPC_release(MinkIPC **me);

/**
   Wrap an fd into a Object.
   This takes ownership of the Fd.
   The caller must relinquish control
   of the descriptor after calling this method.

   int fd: The fd to wrap
   Object* obj: The Obj that represents the fd
**/
void MinkIPC_wrapFd(int fd, Object *obj);

#if defined (__cplusplus)
}
#endif

#endif //__MINKIPC_H
