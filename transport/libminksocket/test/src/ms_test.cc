#include "ISampleService.hpp" // Interface definition to invoke object
#include <gtest/gtest.h>

extern "C" {
#include "minkipc.h"
#include "IOpener.h"
}

// TODO: Ideas for improvements:
// 1. Make this testbench a client AND server
// 2. Have a base class which handles launching client and tests (positive and negative) associated with making a connection.
// 3. Tests at class level?
// 4. Mocks?

#define SLEEP_MS(x)                                       \
    {                                                     \
        printf("\nSleeping for %d milliseconds.\n\n", x); \
        usleep(1000 * x);                                 \
    }

#define SOCK_PATH_A "sim_socket_A"
#define SOCK_PATH_B "sim_socket_B"

// No setup nor tear-down. This class tests public API corner-cases.
class Fundamental : public testing::Test {
protected:
  // local setup, called each time for each instance of test fixture.
  void SetUp() override {
    system("rm -f " SOCK_PATH_A);
    system("rm -f " SOCK_PATH_B);
  }
  // local teardown, called each time for each instance of test fixture.
  void TearDown() override {
    system("rm -f " SOCK_PATH_A);
    system("rm -f " SOCK_PATH_B);
  }

  MinkIPC *mConn1 = nullptr;
  MinkIPC *mConn2 = nullptr;
};

TEST_F(Fundamental, PositiveMultipleServerDiffPath){
  mConn1 = MinkIPC_startService(SOCK_PATH_A, Object_NULL);
  EXPECT_NE(nullptr, mConn1);
  mConn2 = MinkIPC_startService(SOCK_PATH_B, Object_NULL);
  EXPECT_NE(nullptr, mConn2);
  MinkIPC_release(&mConn1);
  MinkIPC_release(&mConn2);
}

TEST_F(Fundamental, NegativeMultipleServerSamePath){
  mConn1 = MinkIPC_startService(SOCK_PATH_A, Object_NULL);
  EXPECT_NE(nullptr, mConn1);
  mConn2 = MinkIPC_startService(SOCK_PATH_A, Object_NULL);
  // 2nd connection cannot be made on a socket which is in use.
  EXPECT_EQ(nullptr, mConn2);
  MinkIPC_release(&mConn1);
}

TEST_F(Fundamental, PositiveMultipleRelease){
  EXPECT_EQ(nullptr, mConn1);
  mConn1 = MinkIPC_startService(SOCK_PATH_A, Object_NULL);
  EXPECT_NE(nullptr, mConn1);
  MinkIPC_release(&mConn1);
  EXPECT_EQ(nullptr, mConn1);
}

TEST_F(Fundamental, PositiveMultipleClientSamePath){
  Object obj[2] = {{0}};
  mConn1 = MinkIPC_startService(SOCK_PATH_A, Object_NULL);
  EXPECT_NE(nullptr, mConn1);
  mConn2 = MinkIPC_connect(SOCK_PATH_A, &obj[0]);
  MinkIPC *mClientConn = MinkIPC_connect(SOCK_PATH_A, &obj[1]);
  Object_ASSIGN_NULL(obj[0]);
  Object_ASSIGN_NULL(obj[1]);
  MinkIPC_release(&mClientConn);
  MinkIPC_release(&mConn2);
  MinkIPC_release(&mConn1);
}

class LoopBack : public testing::Test {
protected:
  // global setup, only called once (for this test suite).
  static void SetUpTestCase() {
    // Where am I?
  }
  // local setup, called each time for each instance of test fixture.
  void SetUp() override {
    mServerConn = MinkIPC_startService(SOCK_PATH_A, Object_NULL);
    ASSERT_NE(nullptr, mServerConn);
    mClientConn = MinkIPC_connect(SOCK_PATH_A, &mOpener);
    ASSERT_NE(nullptr, mClientConn);
  }
  // local teardown, called each time for each instance of test fixture.
  void TearDown() override {
    // Client releases first
    Object_ASSIGN_NULL(mOpener);
    MinkIPC_release(&mClientConn);
    // Followed by the server
    MinkIPC_release(&mServerConn);
  }
private:
  Object mOpener;
  MinkIPC *mServerConn = nullptr;
  MinkIPC *mClientConn = nullptr;
};

TEST_F(LoopBack, PositiveClientRelease) {}

class PointToPoint : public testing::Test {
protected:
  // global setup, only called once (for this test suite).
  static void SetUpTestCase() {
    // Where am I?
    // Find location of this executable so that we can find the client with which it will communicate
    char pBuf[256];
    size_t len = sizeof(pBuf);
    int bytes = readlink("/proc/self/exe", pBuf, len);
    if (bytes >= 0 )
      pBuf[bytes] = '\0';

    // Expect client to have same name + suffix
    char client_suffix[] = "_client";

    std::string cmd_start_client;
    cmd_start_client = pBuf;
    cmd_start_client += client_suffix;
    cmd_start_client += " &";

    // Launch client
    system("touch /tmp/minksocketFullLog");
    system("pkill -9 ms_test_client");
    printf("Starting ms_test_client \n");
    system(cmd_start_client.c_str());

    SLEEP_MS(100);

    printf("%d Starting Test Suite\n", getpid());
  }
  // global teardown, only called once (for this test suite).
  static void TearDownTestCase() {
    system("echo Test Suite completed. Killing ms_test_client ...");
    system("pkill -9 ms_test_client");
    system("rm -f /tmp/minksocketFullLog");
  }

  // local setup, called each time for each instance of test fixture.
  void SetUp() override {
    mConn = MinkIPC_connect("sim_client_socket", &mRawObj);
    // We know what the interface is, so we cast it with a proxy object.
    mSvc = ISampleService(mRawObj);
  }

  // local teardown, called each time for each instance of test fixture.
  void TearDown() override {
    // Clean-up minksocket connection
    if (mConn) {
      MinkIPC_release(&mConn);
    }
    system("rm -f sim_client_socket");
  }

  MinkIPC *mConn = nullptr;
  Object mRawObj = Object_NULL;
  ISampleService mSvc = Object_NULL;
};

/** @brief Send a request to the client
 */
TEST_F(PointToPoint, HelloWorld) {
  EXPECT_EQ(Object_OK, mSvc.printHello());
}
