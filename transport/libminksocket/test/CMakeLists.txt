cmake_minimum_required(VERSION 3.22.1)
project(ms_test)

set(IDL_DIR ${PROJECT_BINARY_DIR}/idl)

file(DOWNLOAD
  https://github.com/quic/mink-idl-compiler/releases/download/v0.2.0/idlc
  ${IDL_DIR}/minkidl
)

file(CHMOD ${IDL_DIR}/minkidl PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)

set(IDLS ISampleService)

# add the custom commands for each document
foreach(IDL ${IDLS})
  foreach( LANG "" "--cpp" )
    if (LANG)
      set(EXT hpp)
    else()
      set(EXT h)
    endif()
    foreach( VARIANT "" "--skel" )
      if (VARIANT)
        set(INVOKE _invoke)
      else()
        set(INVOKE "")
      endif()
      set(RESULT  ${IDL_DIR}/${IDL}${INVOKE}.${EXT} )
      set(INPUT   ${PROJECT_SOURCE_DIR}/idl/${IDL}.idl )
      # message("OUTPUT = ${RESULT}")
      add_custom_command(
        OUTPUT  ${RESULT}
        DEPENDS ${INPUT}
        COMMAND ${IDL_DIR}/minkidl ${INPUT} -o ${RESULT} ${LANG} ${VARIANT}
      )
      set_property(SOURCE ${RESULT} PROPERTY GENERATED ON)
      # build a list of all the results
      list(APPEND RESULTS  ${RESULT} )
    endforeach()
  endforeach()
endforeach()

add_executable(ms_test_client src/main.cpp src/CSampleService.cpp ${RESULTS})
target_include_directories(ms_test_client PRIVATE inc ${IDL_DIR})

target_link_libraries(ms_test_client
  PRIVATE MinkHeaders
  PRIVATE minksocket
)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        6910c9d9165801d8827d628cb72eb7ea9dd538c5 # release-1.16.0, req. v3.22.1
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(ms_test src/ms_test.cc)

target_link_libraries(ms_test
  PRIVATE GTest::gtest_main
  PRIVATE MinkHeaders
  PRIVATE minksocket
)
target_include_directories(ms_test PRIVATE ${IDL_DIR})

if(OFFTARGET)
  target_link_options(ms_test BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address)
endif()

include(GoogleTest)
gtest_discover_tests(ms_test)
