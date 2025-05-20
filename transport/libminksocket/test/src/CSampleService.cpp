/********************************************************************
 Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 All rights reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
*********************************************************************/

#include <iostream> // Include the iostream library for input and output
// #include <stdio.h>
// #include <sys/mman.h>
// #include <sys/socket.h>
// #include <sys/stat.h>
// #include <unistd.h>

// #include "CAppClient.hpp"
// #include "CAppLoader.hpp"
// #include "CDiagnostics.hpp"
// #include "CTMemoryService.hpp"
// #include "CTOEMVMMemoryService.hpp"
// #include "CTOEMVMPowerService.hpp"
// #include "CTPowerService.hpp"
// #include "IAppClient.hpp"
// #include "IAppLoader.hpp"
// #include "IDiagnostics.hpp"
// #include "ISMCIExampleApp.hpp"
#include "CSampleService.hpp"
// #include "ITEnv.hpp"
// #include "ITMemoryService.hpp"
// #include "ITPowerService.hpp"

// extern "C" {
// #include "TUtils.h"
// #include "heap.h"
// #include "moduleAPI.h"
// }

// #define SIZE_4KB 0x1000
// #define SIZE_2MB 0x200000

// IAppLoader gAppLoader;
// IAppController gAppController;
// ISMCIExampleApp gQteeApp;
// // Initialize the global Mink Environment proxy Object so it can be used in this
// // compilation unit. No need to increment gTVMEnv refcount since the proxy
// // destructor is never called.
// ITEnv gTVMEnvProxy = ITEnv(gTVMEnv);

//############### SampleService ###############
int32_t CSampleService::printHello()
{
    // LOG_MSG("Hello world!");
    std::cout << "Hello World" << std::endl; // Print "Hello World" to the console
    return Object_OK;
}

// static int32_t testMemBuf(Object memBuf)
// {
//     uint32_t bufSize = 5 * 1024 * 1024;  // 5MB
//     int32_t ret = Object_ERROR;
//     int32_t bufFd = -1;
//     void *ptr = NULL;

//     LOG_MSG("Testing memory buffer from DMABUFHEAP!");

//     T_GUARD(Object_unwrapFd(memBuf, &bufFd));

//     ptr = mmap(NULL, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
//     T_CHECK(ptr != MAP_FAILED);

//     memset(ptr, 0, bufSize);
//     ret = munmap(ptr, bufSize);

// exit:
//     return ret;
// }

// int32_t CSampleService::getDMABuf()
// {
//     ITMemPoolFactory memPoolFactory = {};
//     ITMemPool memPool = {};
//     IMemObject memBuf[2] = {};
//     uint32_t poolMemSize = 10 * 1024 * 1024;  // 10MB
//     uint32_t bufSize = 5 * 1024 * 1024;       // 5MB
//     int32_t ret = Object_ERROR, idx;
//     ITAccessPermissions_rules defaultRules = {0};
//     defaultRules.specialRules = ITAccessPermissions_keepSelfAccess;

//     LOG_MSG("Start to get DMA buffers!");

//     T_GUARD(gTVMEnvProxy.open(CTMemPoolFactory_UID, memPoolFactory));
//     T_GUARD(memPoolFactory.createPool(defaultRules, poolMemSize, memPool));

//     for (idx = 0; idx < 2; idx++) {
//         T_GUARD(memPool.allocateBuffer(bufSize, memBuf[idx]));
//         T_GUARD(testMemBuf(memBuf[idx].get()));
//     }

// exit:
//     return ret;
// }

// int32_t CSampleService::callQTEETA()
// {
//     int32_t ret = Object_ERROR;
//     IAppClient appClient = {};
//     ISMCIExampleApp app = {};
//     const char appName[] = "smcinvoke_example_ta64";

//     LOG_MSG("Start to call a QTEE TA!");

//     T_GUARD(gTVMEnvProxy.open(CAppClient_UID, appClient));
//     T_GUARD(appClient.getAppObject(&appName, strlen(appName), app));
//     T_GUARD(app.logSinkExample());

// exit:
//     return ret;
// }

// int32_t CSampleService::callQTEEKernelService()
// {
//     int32_t ret = Object_ERROR;
//     IDiagnostics diagnostics = {};
//     IDiagnostics_HeapInfo heapInfo = {};
//     memset((void *)&heapInfo, 0, sizeof(IDiagnostics_HeapInfo));

//     LOG_MSG("Start to call a QTEE kernel service!");

//     T_GUARD(gTVMEnvProxy.open(CDiagnostics_UID, diagnostics));
//     T_GUARD(diagnostics.queryHeapInfo(heapInfo));

//     LOG_MSG("Total bytes as heap: %d", heapInfo.totalSize);

// exit:
//     return ret;
// }

// int32_t CSampleService::shareMemory(const IMemObject &memObj)
// {
//     int32_t ret = Object_ERROR;
//     int32_t fd = -1;
//     uint64_t sz = 0;
//     void *ptr = nullptr;
//     struct stat memObjStat = {};
//     char const message[] = "This string is from the TA";

//     T_GUARD(Object_unwrapFd(memObj.get(), &fd));
//     T_CHECK(fstat(fd, &memObjStat) == 0);
//     T_CHECK(memObjStat.st_size > 0);

//     sz = memObjStat.st_size;
//     ptr = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//     T_CHECK(ptr != MAP_FAILED);

//     snprintf((char *)ptr, sz, "%s", message);
//     LOG_MSG("The message back to the caller: %s", (char *)ptr);

// exit:
//     if (ptr != nullptr) munmap(ptr, sz);

//     return ret;
// }

// int32_t CSampleService::loadQTEETA(const IMemObject &appFileObj)
// {
//     int32_t ret = Object_ERROR;

//     LOG_MSG("Start to load a QTEE TA!");

//     T_GUARD(gTVMEnvProxy.open(CAppLoader_UID, gAppLoader));
//     T_GUARD(gAppLoader.loadFromRegion(appFileObj, gAppController));
//     T_GUARD(gAppController.getAppObject(gQteeApp));

// exit:
//     return ret;
// }

// int32_t CSampleService::shareMemWithQTEETA(uint32_t uid)
// {
//     int32_t ret = Object_ERROR, bufFd = -1;
//     uint32_t bufSize = SIZE_4KB;
//     uint32_t poolMemSize = (bufSize + (SIZE_2MB - 1)) & (~(SIZE_2MB - 1));
//     ITMemPoolFactory memPoolFactory = {};
//     ITMemPool memPool = {};
//     IMemObject memBuf = {};
//     ITAccessPermissions_rules confRules = {0};
//     confRules.specialRules = ITAccessPermissions_keepSelfAccess;
//     char const message[] = "This string is from the TVM TA";
//     void *ptr = nullptr;

//     T_GUARD(gTVMEnvProxy.open(uid, memPoolFactory));
//     T_GUARD(memPoolFactory.createPool(confRules, poolMemSize, memPool));
//     T_GUARD(memPool.allocateBuffer(bufSize, memBuf));

//     T_GUARD(Object_unwrapFd(memBuf.get(), &bufFd));
//     ptr = mmap(nullptr, bufSize, PROT_READ | PROT_WRITE, MAP_SHARED, bufFd, 0);
//     T_CHECK(ptr != MAP_FAILED);

//     snprintf((char *)ptr, bufSize, "%s", message);

//     T_GUARD(gQteeApp.sharedMemoryExample(memBuf));

//     ((char *)ptr)[bufSize - 1] = '\0';
//     LOG_MSG("Modified buffer: %s", (char *)ptr);

// exit:
//     if (ptr != NULL) munmap(ptr, bufSize);

//     return ret;
// }

// int32_t CSampleService::acquireWakeLock(uint32_t uid)
// {
//     int32_t ret = Object_ERROR;
//     ITPowerService powerService = {};

//     T_CHECK(uid == CTPowerService_UID || uid == CTOEMVMPowerService_UID);

//     LOG_MSG("Start to acquire the wake lock!");

//     T_GUARD(gTVMEnvProxy.open(uid, powerService));

//     if (uid == CTPowerService_UID) {
//         T_CHECK(qtvmWakeLock == nullptr);
//         qtvmWakeLock = new ITWakeLock();
//         T_GUARD(powerService.acquireWakeLock(*qtvmWakeLock));
//     } else {
//         T_CHECK(oemvmWakeLock == nullptr);
//         oemvmWakeLock = new ITWakeLock();
//         T_GUARD(powerService.acquireWakeLock(*oemvmWakeLock));
//     }

// exit:
//     return ret;
// }

// int32_t CSampleService::releaseWakeLock()
// {
//     if (qtvmWakeLock != nullptr) {
//         delete qtvmWakeLock;
//         qtvmWakeLock = nullptr;
//     }

//     if (oemvmWakeLock != nullptr) {
//         delete oemvmWakeLock;
//         oemvmWakeLock = nullptr;
//     }

//     return Object_OK;
// }

int32_t CSampleService::releaseConnection()
{
    std::cout << "Releasing connection from CSampleSevice" << std::endl;
    // NOTE! Normally services don't have access to the MinkIPC connection.
    // Special care was given to test negative use-cases.
    MinkIPC_release(&gConnection);
    return Object_OK;
}

#ifdef __cplusplus
extern "C" {
#endif

int32_t CSampleService_open(uint32_t uid, Object credentials, Object *objOut)
{
    int32_t ret = Object_OK;
    CSampleService *me = new CSampleService(credentials);

    // T_CHECK_ERR(me != NULL, Object_ERROR_MEM);

    // LOG_MSG("Opening service with UID = 0x%x", uid);
    std::cout << "Opening service with UID = 0x%x" << uid << std::endl; // Print "Hello World" to the console

    *objOut = (Object){ImplBase::invoke, me};

exit:
    if (ret) {
        delete me;
    }

    return ret;
}

#ifdef __cplusplus
}
#endif
