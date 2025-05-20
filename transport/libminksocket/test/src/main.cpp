#include "CSampleService.hpp" // Class definition to instantiate
#include "ISampleService.hpp" // Interface definition to invoke object
#include "minkipc.h"
#include <iostream> // Include the iostream library for input and output
#include <unistd.h> // For getting PID of self

// extern "C" {
// #include "CSampleService_open.h"
// }

MinkIPC *gConnection = nullptr;

/***********************************************************************
 * You guessed it, main
 * ********************************************************************/
int32_t main(int32_t argc, char *argv[]) {
  int32_t ret = Object_OK;
  Object rawObj = Object_NULL;
  // ISampleService cppObj = {};

  // 0. Print diagnostics
  std::cout << "server PID = " << getpid() << std::endl;

  // 1. Create Mink Object to bind to UNIX socket
  rawObj = (Object){ImplBase::invoke, new CSampleService()};

  // // 2. Test to see that it works
  // ISampleService cppObj = ISampleService(rawObj);
  // cppObj.printHello();

  // 3. Startup Unix server. Defined in CSampleService.hpp for negative testing.
  gConnection = MinkIPC_startService("sim_client_socket", rawObj);
  MinkIPC_join(gConnection);

  std::cout << "all finished in " << argv[0] << std::endl;
  MinkIPC_release(&gConnection);
  return ret;
}
