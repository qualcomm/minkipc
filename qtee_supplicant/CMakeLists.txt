project(qtee_supplicant C)

# ''Packages''.

# Append modules directory.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(QCBOR_DIR_HINT "" CACHE PATH "Hint path for QCBOR directory")

find_package(QCBOR REQUIRED)
if(NOT QCBOR_FOUND)
	message(FATAL_ERROR "QCBOR not found")
endif()

include_directories(${QCBOR_INCLUDE_DIRS})

# ''Source files''.

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SRC
	src/qtee_supplicant.c
	src/listener_mngr.c
	src/CListenerCBO.c
)

# ''Built binary''.

add_executable(${PROJECT_NAME} ${SRC})

# ''Headers and dependencies''.

target_include_directories(${PROJECT_NAME}
	PRIVATE src
	PRIVATE include
	PRIVATE idl
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE minkadaptor
	PRIVATE ${QCBOR_LIBRARIES}
	PRIVATE dl
)

install(TARGETS ${PROJECT_NAME} DESTINATION "${CMAKE_INSTALL_BINDIR}")
