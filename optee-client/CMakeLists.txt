project(ckqteec
	VERSION 0.1.0
	LANGUAGES C)

option(CFG_WERROR "Build with -Werror" TRUE)

include(GNUInstallDirs)

################################################################################
# Compiler flags:
#   We want to use the same flags in the entire optee_client git
################################################################################
add_compile_options(
	-Wall -Wbad-function-cast -Wcast-align
	-Werror-implicit-function-declaration -Wextra
	-Wfloat-equal -Wformat-nonliteral -Wformat-security
	-Wformat=2 -Winit-self -Wmissing-declarations
	-Wmissing-format-attribute -Wmissing-include-dirs
	-Wmissing-noreturn -Wmissing-prototypes -Wnested-externs
	-Wpointer-arith -Wshadow -Wstrict-prototypes
	-Wswitch-default -Wwrite-strings -fPIC
	-Wno-unterminated-string-initialization
)
if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(
        -Wunsafe-loop-optimizations
    )
endif()
if(CFG_WERROR)
    add_compile_options(-Werror)
endif(CFG_WERROR)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

################################################################################
# Packages
################################################################################
# Use the libmink* targets when built as part of the top-level project (they are
# not installed yet), otherwise locate the installed ones via pkg-config.
if(TARGET minkteec AND TARGET minkadaptor)
	set(MINK_LIBS minkteec minkadaptor)
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(MINKTEEC REQUIRED IMPORTED_TARGET minkteec)
	pkg_check_modules(MINKADAPTOR REQUIRED IMPORTED_TARGET minkadaptor)
	set(MINK_LIBS PkgConfig::MINKTEEC PkgConfig::MINKADAPTOR)
endif()
find_package(Threads REQUIRED)
if(NOT THREADS_FOUND)
	message(FATAL_ERROR "Threads not found")
endif()

include(GNUInstallDirs)

################################################################################
# Source files
################################################################################
set(SRC
	optee_client/libckteec/src/pkcs11_api.c
	optee_client/libckteec/src/ck_debug.c
	optee_client/libckteec/src/ck_helpers.c
	optee_client/libckteec/src/invoke_ta.c
	optee_client/libckteec/src/pkcs11_processing.c
	optee_client/libckteec/src/pkcs11_token.c
	optee_client/libckteec/src/serializer.c
	optee_client/libckteec/src/serialize_ck.c
	src/teec_trace.c
)

################################################################################
# Built library
################################################################################
add_library(ckqteec SHARED ${SRC})

set_target_properties(ckqteec PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

################################################################################
# Flags always set
################################################################################
target_compile_definitions(ckqteec
	PRIVATE -D_GNU_SOURCE
	PRIVATE -DBINARY_PREFIX="LT"
)

################################################################################
# Optional flags
################################################################################

################################################################################
# Public and private header and library dependencies
################################################################################
target_include_directories(ckqteec
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/optee_client/libckteec/include>
	$<INSTALL_INTERFACE:optee_client/libckteec/include>
	PRIVATE optee_client/libckteec/src
	PRIVATE ${CMAKE_SOURCE_DIR}/libminkteec/include
	PRIVATE include
)

target_link_libraries(ckqteec
	PRIVATE pthread
	PRIVATE ${MINK_LIBS}
	PRIVATE m
)

################################################################################
# Install targets
################################################################################
install(TARGETS ckqteec
	DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

add_subdirectory(optee_client/libckteec/include)
