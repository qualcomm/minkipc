// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#pragma once
#include <cstring>
#include <fstream>
#include <list>
#include <string>
#include <sys/mman.h>
#include <vector>

#include "dmaMemPool.h"
#include "object.h"

/**
 * @brief taImageStatus : TA Image status code
 *
 * Enum specifying the Current status of TAImage
 * kTAImageFound: TA Image Found
 * kErrTAImageNotFound: TA Image Not Found.
 * KErrTAImageReserved: Reserved.
 */
enum class taImageStatus : int
{
  kErrOk = 0,
  kErrInternal,
  kErrBuffAllocateFailed,
  kErrImageNotFound,
  kErrBuffReleaseFailed,
  kBuffAllocated,
};

/**
 * @brief TAImageReader : Class containing Image Buffer for TA
 * @members
 *         @private mImageFile: fstream Objec to TA Image file
 *                  mMemBuffer: struct for Memory Buffer
 *                  mBufferStatus: bufferStatus Status for bufferstatus
 *         @methods getImageMbnFile(): Create A Buffer from TA MBN Image
 *                  ReadSplitBinsToBuf(): Read All split bins into a buffer
 *                  taImageStatus createImageBuffer(): Create DMA Heap TA Buffer
 *         @public
 *         @methods
 *           int getMemoryObject(): get the Memory Object
 *           taImageStatus checkTABufferStatus(); get TA Status
 */
class TAImageReader
{
  private:
    std::fstream mImageFile;
    MemoryBuffer * mMemBuffer;
    taImageStatus mBufferStatus;
    //@internal methods
    bool getImageMbnFile(std::string path, size_t fileSize, Object rootObj);
    bool ReadSplitBinsToBuf(std::string &path, Object rootObj);
    taImageStatus createImageBuffer(size_t buffLen, Object rootObj);
    TAImageReader(std::list<std::string> searchPaths, Object rootObj, std::string uuid);
    //@override
    taImageStatus createImageBuffer(std::vector<uint8_t> *rawData, Object rootObj);
  public:
    //@getters
    Object getMemoryObject(){ return mMemBuffer->memObj; };
    taImageStatus checkTABufferStatus(){ return mBufferStatus; };
    static taImageStatus createTAImageReader(std::list<std::string> searchPaths,
                                             Object rootObj,
                                             std::string uuid,
                                             TAImageReader ** imageObj);
    //delete default and copy constructors
    TAImageReader() = delete;
    TAImageReader(const TAImageReader &) = delete;
    ~TAImageReader();
};
