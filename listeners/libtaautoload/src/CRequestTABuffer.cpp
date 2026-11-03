// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "CRequestTABuffer.h"

#include "IRequestTABuffer_invoke.h"
#include "IRequestTABuffer.h"
#include "taImageReader.h"
#include "utils.h"
#include <fstream>
#include <string>
#include <vector>

using namespace std;

/* TEEC UUID STRING */
static const size_t HYPHEN_LEN(4);
static const size_t NULLCHAR_LEN(1);
static const size_t STRING_MULTIPLIER(2);

/* TA binaries path on device, set at build time from the TA_DIR CMake option */
#ifndef TA_PATH
#error "TA_PATH is not defined: set the TA binaries path with -DTA_DIR=<path>"
#endif

/**
 * @brief Parse the device compatible string for possible device names.
 *
 * @return A vector for possible device names.
 */
static vector<string> parseCompatibleList(const string& raw)
{
    vector<string> result;
    size_t start = 0;

    while (start < raw.size())
    {
        size_t end = raw.find('\0', start);
        if (end == string::npos)
            break;

        string entry = raw.substr(start, end - start);

        // Find vendor prefix separator
        size_t comma_pos = entry.find(',');

        if (comma_pos != string::npos && comma_pos + 1 < entry.size())
        {
            // Strip prefix 'qcom'
            result.emplace_back(entry.substr(comma_pos + 1));
        }
        else
        {
            // No vendor prefix present
            result.emplace_back(entry);
        }

        start = end + 1;
    }

    return result;
}

/**
 * @brief Read the device compatible string
 *
 * @return Device compatible string delimited by \0.
 */
static string readCompatibleString()
{
    const char* path = "/sys/firmware/devicetree/base/compatible";

    ifstream file(path, ios::in | ios::binary);
    if (!file.is_open())
    {
        return {};
    }

    // Read entire file into a string
    string data((std::istreambuf_iterator<char>(file)),
                 std::istreambuf_iterator<char>());

    return data;
}

/**
 * @brief Create CRequestTABBuffer and prepare TA binary search paths
 *
 * @return CRequestTABuffer object.
 */
static CRequestTABuffer *getCRequestTABuffer()
{
  CRequestTABuffer * requestTABuff = new CRequestTABuffer();
  string taPath = TA_PATH;
  /* append / in case not present at the end */
  if('/' != taPath.back())
  {
    taPath = taPath + string("/");
  }

  string raw = readCompatibleString();
  if (raw.empty())
  {
    MSGE("Could not read device compatible string!\n");
    return requestTABuff;
  }

  auto vec = parseCompatibleList(raw);
  if (vec.empty())
  {
    MSGE("No device names found in compatible!\n");
    return requestTABuff;
  }

  for (const auto& entry : vec)
  {
    string updatedTaPath = taPath + entry + string("/");
    requestTABuff->searchLocations.push_back(updatedTaPath);
    MSGD("Path %s\n", updatedTaPath.c_str());
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
