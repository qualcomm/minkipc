// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "CRequestTABuffer.h"

#include "IRequestTABuffer_invoke.h"
#include "IRequestTABuffer.h"
#include "taImageReader.h"
#include "utils.h"

using namespace std;

/* TEEC UUID STRING */
static const size_t HYPHEN_LEN(4);
static const size_t NULLCHAR_LEN(1);
static const size_t STRING_MULTIPLIER(2);

/**
 * @brief Create CRequestTABBuffer and prepare TA binary search paths
 *
 * @return CRequestTABuffer object.
 */
static CRequestTABuffer *getCRequestTABuffer()
{
  CRequestTABuffer * requestTABuff = new CRequestTABuffer();
  for (int i = 0; i < TA_PATH_LIST_SIZE; i++)
  {
    string taPaths = ta_path_list[i];
    /* append / in case not present at the end */
    if('/' != taPaths.at(taPaths.length() - 1))
    {
      taPaths = taPaths + string("/");
    }
    requestTABuff->searchLocations.push_back(taPaths);
    MSGD("Path %s\n", taPaths.c_str());
  }
  return requestTABuff;
}

/**
 * @brief Retain the self Object.
 *
 * @param me The local object associated with this interface.
 * @return Object_OK
 */
static int32_t CRequestTABuffer_retain(CRequestTABuffer *me)
{
  atomic_add(&me->refs, 1);
  return Object_OK;
}

/**
 * @brief Release the self Object.
 *
 * @param me The local object associated with this interface.
 * @return Object_OK
 */
static int32_t CRequestTABuffer_release(CRequestTABuffer *me)
{
  if (atomic_add(&me->refs, -1) == 0)
  {
    Object_ASSIGN_NULL(me->rootObj);
    delete(me);
  }
  return Object_OK;
}

int32_t CRequestTABuffer_get(CRequestTABuffer *me, const void *uuid_ptr, size_t uuid_len,
			     Object *appElf)
{
  int32_t retVal = Object_ERROR;
  char *distName = NULL;

  /* Load Image finding from search location */
  size_t distNameLen = 0;
  TAImageReader *TAImage = nullptr;
  TEEC_UUID *pTargetUUID = NULL;
  if (uuid_len != sizeof(TEEC_UUID))
  {
    MSGE("Invalid UUID Len");
    goto ERROR_HANDLE;
  }

  distNameLen = sizeof(TEEC_UUID) * STRING_MULTIPLIER + HYPHEN_LEN + NULLCHAR_LEN;
  pTargetUUID = (TEEC_UUID *)uuid_ptr;
  distName = new char [distNameLen];

  snprintf(distName, distNameLen,
    "%08X-%04X-%04X-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
    pTargetUUID->timeLow, pTargetUUID->timeMid,
    pTargetUUID->timeHiAndVersion, pTargetUUID->clockSeqAndNode[0],
    pTargetUUID->clockSeqAndNode[1], pTargetUUID->clockSeqAndNode[2],
    pTargetUUID->clockSeqAndNode[3], pTargetUUID->clockSeqAndNode[4],
    pTargetUUID->clockSeqAndNode[5], pTargetUUID->clockSeqAndNode[6],
    pTargetUUID->clockSeqAndNode[7]);

  MSGD("UUID Name %s\n", distName);
  /* Create a New Image Object */
  if(taImageStatus::kErrOk != TAImageReader::createTAImageReader(me->searchLocations,
                                                                 me->rootObj,
                                                                 string(distName), &TAImage))
  {
      goto ERROR_HANDLE_IMAGE;
  }

  Object_INIT(*appElf, TAImage->getMemoryObject());

  retVal = Object_OK;

ERROR_HANDLE_IMAGE:
  delete[](distName);
  /* Ummap the Image after handling the MemObj in case of success */
  if(nullptr != TAImage)
  {
    /* call destructor only in case TA Buffer was allocated */
    delete(TAImage);
  }
ERROR_HANDLE:
  return retVal;
}

static IRequestTABuffer_DEFINE_INVOKE(IRequestTABuffer_invoke, CRequestTABuffer_, CRequestTABuffer *)

int32_t CRequestTABuffer_open(Object *RequestTABufferObj_ptr, Object rootObj)
{
  int32_t retVal = Object_ERROR;
  CRequestTABuffer *me = getCRequestTABuffer();
  if(me == nullptr)
  {
    MSGE("Failed to parse Configuration");
    goto ERROR_HANDLE;
  }

  me->refs = 1;
  Object_INIT(me->rootObj, rootObj);

  *RequestTABufferObj_ptr = (Object) {IRequestTABuffer_invoke, me};
  return Object_OK;

ERROR_HANDLE:
  delete(me);
  return retVal;
}
