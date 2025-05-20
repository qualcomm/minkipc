#include "ICredentials.hpp"
#include "ISampleService_invoke.hpp"

#include "minkipc.h"
extern MinkIPC *gConnection;

class CSampleService : public ISampleServiceImplBase
{
   public:
    CSampleService(){};
    explicit CSampleService(Object cred)
    {
        // Increment the cred refcount before being consumed by proxy object.
        // Member proxy objects will be cleaned up when this instance is
        // destroyed.
        Object_retain(cred);
        this->mCredentials = ICredentials(cred);
        // this->qtvmWakeLock = nullptr;
        // this->oemvmWakeLock = nullptr;
    };
    virtual ~CSampleService(){};

    virtual int32_t printHello();
    // virtual int32_t getDMABuf();
    // virtual int32_t callQTEETA();
    // virtual int32_t callQTEEKernelService();
    // virtual int32_t shareMemory(const IMemObject &memObj);
    // virtual int32_t loadQTEETA(const IMemObject &appFileObj);
    // virtual int32_t shareMemWithQTEETA(uint32_t uid);
    // virtual int32_t acquireWakeLock(uint32_t uid);
    // virtual int32_t releaseWakeLock();
    virtual int32_t releaseConnection();

   private:
    ICredentials mCredentials;
    // ITWakeLock *qtvmWakeLock;
    // ITWakeLock *oemvmWakeLock;
};
