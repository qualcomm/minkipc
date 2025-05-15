// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <fstream>
#include <linux/elf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#include "taImageReader.h"
#include "dmaMemPool.h"
#include "utils.h"

using namespace std;

/* Elf Header Format */
typedef union
{
  Elf32_Ehdr Elf32;
  Elf64_Ehdr Elf64;
} ElfHdr;
ElfHdr *pElfHdr;

/**
 * @brief Create TA Buffer from TA Image file
 *
 * @param searchPaths list of seearch path to TA image locations
 * @param rootObj A root object for initiating communication with QTEE.
 * @param uuid uuid for TA to load
 * @param imageObj image reader object representing the TA image.
 * @return kErrOk on success.
 *         kErr* on failure.
 */
taImageStatus TAImageReader::createTAImageReader(std::list<std::string> searchPaths,
                                                 Object rootObj,
                                                 std::string uuid,
                                                 TAImageReader **imageObj)
{
  taImageStatus retVal = taImageStatus::kErrImageNotFound;
  *imageObj  = new TAImageReader(searchPaths, rootObj, uuid);
  retVal = (*imageObj)->checkTABufferStatus();
  if(taImageStatus::kErrOk != retVal)
  {
    MSGE("Failed to construct Buffer from TA with uid %s"
         " Error Code %d\n",uuid.c_str(), (int)retVal);

      delete(*imageObj);
      *imageObj = nullptr;
  }
  return retVal;
}

/**
 * @brief Read the split TA binaries to buffer.
 *
 * @param path Path to split binary location.
 * @param rootObj A root object for initiating communication with QTEE.
 * @return true if splitbins are loaded.
 *         false otherwise.
 */
bool TAImageReader::ReadSplitBinsToBuf(string &path, Object rootObj)
{
  bool retVal = false;
  bool is64 = false;
  size_t phdrTableOffset=0, binOffset = 0, bufferOffset = 0;
  fstream splitbin;
  int file_size = 0;
  vector<uint8_t> imageBuffer;
  vector<size_t> offset;
  Elf64_Phdr phdr64;
  Elf32_Phdr phdr32;

  /* for each of the remaining segments, read into buffer at offset[seg] */
  size_t pathLen = path.length();

  /* open b00 file as inputFile */
  mImageFile.open(path, mImageFile.in | mImageFile.binary | mImageFile.ate);
  if(!mImageFile.is_open())
  {
    MSGE("Failed to open b00 File\n");
    goto ERROR_HANDLE;
  }

  MSGD("Opened %s.b00", path.c_str());

  file_size = (int) mImageFile.tellg();
  if(file_size <= 0)
  {
    MSGE("Invalid b00 Size\n");
    goto ERROR_HANDLE;
  }

  /* Reset to beg */
  mImageFile.seekg(0, mImageFile.beg);
  MSGE("ReadSplitBinsToBuf buffer->size()=%d\n", file_size);
  imageBuffer.reserve(file_size);
  imageBuffer.assign((istreambuf_iterator<char>(mImageFile)), istreambuf_iterator<char>());


  pElfHdr = (ElfHdr*) (void*) (imageBuffer.data());
  offset.resize(pElfHdr->Elf64.e_phnum);

  /* Examine buffer's contents as an elf file, check segment count
   * and get offsets of remaining segments
   */
  if (imageBuffer.size() < sizeof(ElfHdr))
  {
    MSGE("ReadSplitBinsToBuf buffer->size()=%zu\n", imageBuffer.size());
    goto ERROR_HANDLE;
  }

  switch (pElfHdr->Elf32.e_ident[EI_CLASS])
  {
    case ELFCLASS32:  is64 = false; break;
    case ELFCLASS64:  is64 = true; break;
    default:
      MSGE("Unknown File Type\n");
      return false;
  }

  if (is64)
  {
    phdrTableOffset = (size_t) pElfHdr->Elf64.e_phoff;
    for (size_t phi = 1; phi < pElfHdr->Elf64.e_phnum; ++phi)
    {
      bufferOffset = phdrTableOffset + phi * sizeof(Elf64_Phdr);
      memscpy(&phdr64, sizeof(Elf64_Phdr), imageBuffer.data() + bufferOffset, imageBuffer.size() - bufferOffset);
      offset[phi] = (size_t) phdr64.p_offset;
    }
  }
  else
  {
    phdrTableOffset = pElfHdr->Elf32.e_phoff;
    for (size_t phi = 1; phi < pElfHdr->Elf64.e_phnum; ++phi)
    {
      bufferOffset = phdrTableOffset + phi * sizeof(Elf32_Phdr);
      memscpy(&phdr32, sizeof(Elf32_Phdr), imageBuffer.data() + bufferOffset, imageBuffer.size() - bufferOffset);
      offset[phi] = (size_t) phdr32.p_offset;
    }
  }

  /* Iterate all Program headers */
  for (size_t programSegments = 1; programSegments < pElfHdr->Elf64.e_phnum; ++programSegments)
  {
    path[pathLen - 1] = (char)('0' + programSegments);
    /* Read segment into buffer starting at offset[i] */
    splitbin.open(path.c_str(), splitbin.in | splitbin.binary | splitbin.ate);
    if(splitbin.is_open())
    {
      file_size = (int) splitbin.tellg();
      /* Reset ifstream to beginning of the file */
      splitbin.seekg(0, splitbin.beg);
      /* Copy file's contents into buffer at specified offset */
      binOffset = offset[programSegments];
      if((file_size + binOffset) > imageBuffer.size())
      {
        imageBuffer.resize(file_size + binOffset);
        pElfHdr = (ElfHdr*) (void*) (imageBuffer.data());
      }
      std::copy((istreambuf_iterator<char>(splitbin)), istreambuf_iterator<char>(),
              &(imageBuffer[binOffset]));
      splitbin.close();
    }
    else
    {
      goto ERROR_HANDLE;
    }
  }
  /* Create DMA Buffer */
  if (taImageStatus::kBuffAllocated != createImageBuffer(&imageBuffer, rootObj))
  {
    MSGE("Failed to Allocate Buffer\n");
    retVal = false;
    goto ERROR_HANDLE;
  }
  /* Success */
  retVal = true;
ERROR_HANDLE:
  imageBuffer.clear();
  /* Shrink memory to zero */
  imageBuffer.shrink_to_fit();
  offset.clear();
  mImageFile.close();
  return retVal;
}

/**
 * @brief Read the MBN format TA binary to buffer.
 *
 * @param imagePath Path to binary location.
 * @param fileSize Size of the binary file to read.
 * @param rootObj A root object for initiating communication with QTEE.
 * @return true if binary is found.
 *         false otherwise.
 */
bool TAImageReader::getImageMbnFile(string imagePath, size_t fileSize, Object rootObj)
{
  bool retVal = false;

  /* Check if File was found and open was successfull */
  mImageFile.open(imagePath.c_str(), mImageFile.in | mImageFile.ate | mImageFile.binary);
  if(false == mImageFile.is_open())
  {
    MSGE("File cannot be opened\n");
    goto ERROR_HANDLE;
  }

  MSGD("Opened %s\n", imagePath.c_str());

  mImageFile.seekg(0, mImageFile.beg);
  if (taImageStatus::kBuffAllocated != createImageBuffer(fileSize, rootObj))
  {
    MSGE("Failed to Allocate Buffer\n");
    goto ERROR_HANDLE;
  }

  /* Success */
  retVal = true;

ERROR_HANDLE:
  mImageFile.close();
  return retVal;
}

/**
 * @brief Create an Image Buffer from raw data.
 *
 * @param rawData Pointer to vector containing the raw image data.
 * @param rootObj A root object for initiating communication with QTEE.
 * @return kErrOk on success.
 *         kErr* on failure.
 */
taImageStatus TAImageReader::createImageBuffer(vector<uint8_t> *rawData, Object rootObj)
{
  /* Create DMA Buffer */
  if(MEM_OP_SUCCESS != DMAMemPoolGetBuff(mMemBuffer, rawData->size(), rootObj))
  {
    MSGE("Failed to allocated DMA Buff Heap Memeory\n");
    mBufferStatus = taImageStatus::kErrBuffAllocateFailed;
    goto ERROR_HANDLE;
  }

  memscpy(mMemBuffer->memBuf, mMemBuffer->bufferLen, rawData->data(), rawData->size());

  /* Success */
  mBufferStatus = taImageStatus::kBuffAllocated;

ERROR_HANDLE:
  return mBufferStatus;
}

/**
 * @brief Create an Image Buffer from binary file.
 *
 * @param buffLen Size of the required Image buffer.
 * @param rootObj A root object for initiating communication with QTEE.
 * @return kErrOk on success.
 *         kErr* on failure.
 */
taImageStatus TAImageReader::createImageBuffer(size_t buffLen, Object rootObj)
{
   mBufferStatus = taImageStatus::kErrBuffAllocateFailed;

  /* Buffer Allocated copy image data into Buffer */
  if(MEM_OP_SUCCESS != DMAMemPoolGetBuff(mMemBuffer, buffLen, rootObj))
  {
    MSGE("Failed to allocated DMA Buff Heap Memeory\n");
    goto ERROR_HANDLE;
  }

  /* Memory Allocated */
  mImageFile.read((char *)mMemBuffer->memBuf, mMemBuffer->bufferLen);

  /* Success */
  mBufferStatus = taImageStatus::kBuffAllocated;

ERROR_HANDLE:
  return mBufferStatus;
}

/* @constructor */
TAImageReader::TAImageReader(std::list<std::string> searchPaths, Object rootObj, std::string uuid):
			     mMemBuffer(nullptr), mBufferStatus(taImageStatus::kErrImageNotFound)
{
  bool imageFound = false;
  string pathName = "";
  if(searchPaths.empty())
  {
    /* Return Image not Found */
    MSGE("Empty TA Path list\n");
    return;
  }
  mMemBuffer = new MemoryBuffer();
  for(list<std::string>::iterator it = searchPaths.begin(); it != searchPaths.end(); it++)
  {
    pathName = *it + uuid + string(".mbn");
    struct stat buffer;
    if(0 != stat(pathName.c_str(), &buffer))
    {
      MSGE("%s.mbn File not found @ %s\n", uuid.c_str(), pathName.c_str());
      pathName = *it + uuid + string(".b00");
      if(0 != stat(pathName.c_str(), &buffer))
      {
        MSGE("%s.b00 also File not found @ %s\n", uuid.c_str(), pathName.c_str());
      }
      else
      {
        imageFound = ReadSplitBinsToBuf(pathName, rootObj);
        if(true == imageFound)
        {
          if(taImageStatus::kErrBuffAllocateFailed == mBufferStatus)
          {
            MSGE("Splitbin Buffer Allocation Failed for Image\n");
          }
          else
          {
            mBufferStatus = taImageStatus::kErrOk;
          }
          break;
        }
      }
    }
    else
    {
      imageFound = getImageMbnFile(pathName, buffer.st_size, rootObj);
      if(true == imageFound)
      {
          if(taImageStatus::kErrBuffAllocateFailed == mBufferStatus)
          {
            MSGE("Mbn Buffer Allocation Failed for Image\n");
          }
          else
          {
            mBufferStatus = taImageStatus::kErrOk;
          }
          break;
      }
    }
  }
}

/* @destructor */
TAImageReader::~TAImageReader()
{
  if((nullptr != mMemBuffer))
  {

    if(MEM_OP_SUCCESS != DMAMemPoolReleaseBuff(mMemBuffer))
    {
      MSGE("Failed to release Buffer\n");
    }
  }
}
