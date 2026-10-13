project (xtest_qtee C)

option(CFG_WERROR "Build with -Werror" FALSE)
option(CFG_PKCS11_TA "Build with PKCS11" TRUE)

set (OPTEE_TEST_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/optee_test)
################################################################################
# Compiler flags:
#   We want to use the same flags in the entire optee_client git
################################################################################
add_compile_options (
	-Wall -Wbad-function-cast -Wcast-align
	-Werror-implicit-function-declaration -Wextra
	-Wfloat-equal -Wformat-nonliteral -Wformat-security
	-Wformat=2 -Winit-self -Wmissing-declarations
	-Wmissing-format-attribute -Wmissing-include-dirs
	-Wmissing-prototypes -Wnested-externs
	-Wpointer-arith -Wshadow -Wstrict-prototypes
	-Wswitch-default -Wunsafe-loop-optimizations
	-Wwrite-strings -fPIC
 	-Wno-missing-field-initializers
	-Wno-unused-parameter -Wno-alloc-size
	-Wno-unterminated-string-initialization
)
if(CFG_WERROR)
    add_compile_options(-Werror)
endif(CFG_WERROR)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

add_subdirectory (optee_test/ta)

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

find_package(OpenSSL)
if(OPENSSL_FOUND)
	add_compile_options(-DOPENSSL_FOUND=1 -DOPENSSL_API_COMPAT=10100)
	set (OPENSSL_PRIVATE_LINK OpenSSL::Crypto)
endif()

find_package(Python REQUIRED)

include(GNUInstallDirs)

macro(EMBED_8100FILE prefix infile)
	add_custom_command(
		OUTPUT  regression_8100_${prefix}.h
		COMMAND ${Python_EXECUTABLE} ${OPTEE_TEST_ROOT_DIR}/scripts/file_to_c.py --inf ${infile}
	--out ${CMAKE_CURRENT_BINARY_DIR}/regression_8100_${prefix}.h
	--name regression_8100_${prefix}
		DEPENDS ${OPTEE_TEST_ROOT_DIR}/scripts/file_to_c.py ${infile}
	)

	set_property(SOURCE optee_test/host/xtest/regression_8100.c APPEND PROPERTY OBJECT_DEPENDS
		     ${CMAKE_CURRENT_BINARY_DIR}/regression_8100_${prefix}.h)
endmacro(EMBED_8100FILE)

EMBED_8100FILE(ca_crt ${OPTEE_TEST_ROOT_DIR}/cert/ca.crt)
EMBED_8100FILE(mid_crt ${OPTEE_TEST_ROOT_DIR}/cert/mid.crt)
EMBED_8100FILE(my_crt ${OPTEE_TEST_ROOT_DIR}/cert/my.crt)
EMBED_8100FILE(my_csr ${OPTEE_TEST_ROOT_DIR}/cert/my.csr)

set (SRC
	optee_test/host/xtest/adbg/src/adbg_case.c
	optee_test/host/xtest/adbg/src/adbg_enum.c
	optee_test/host/xtest/adbg/src/adbg_expect.c
	optee_test/host/xtest/adbg/src/adbg_log.c
	optee_test/host/xtest/adbg/src/adbg_run.c
	optee_test/host/xtest/adbg/src/security_utils_hex.c
	optee_test/host/xtest/aes_perf.c
	optee_test/host/xtest/benchmark_1000.c
	optee_test/host/xtest/benchmark_2000.c
	optee_test/host/xtest/clear_storage.c
	optee_test/host/xtest/regression_1000.c
	optee_test/host/xtest/regression_4000.c
	optee_test/host/xtest/regression_4100.c
	optee_test/host/xtest/regression_5000.c
	optee_test/host/xtest/regression_6000.c
	optee_test/host/xtest/regression_8000.c
	optee_test/host/xtest/regression_8100.c
	optee_test/host/xtest/hash_perf.c
	optee_test/host/xtest/stats.c
	optee_test/host/xtest/xtest_helpers.c
	optee_test/host/xtest/xtest_main.c
	optee_test/host/xtest/xtest_test.c
	optee_test/host/xtest/xtest_uuid_helpers.c
)

if (WITH_GP_TESTS)
	add_compile_options(-DWITH_GP_TESTS=1)

	macro(__GEN_GP_FILE outfile xmldir name basedir)
		add_custom_command(
			OUTPUT	${outfile}
			COMMAND	xalan -in ${basedir}/${name}.xml
				      -xsl gp/${name}.xsl -out ${outfile}
			DEPENDS ${basedir}/${name}.xml gp/${name}.xsl
		)
	endmacro(__GEN_GP_FILE)
	macro(GEN_GP_FILE outfile xmldir name)
		__GEN_GP_FILE(${outfile} ${xmldir} ${name}
			      gp-suite/packages/${xmldir})
		list (APPEND SRC ${outfile})
	endmacro(GEN_GP_FILE)

	GEN_GP_FILE(gp_10000.c ClientAPI/xmlstable TEE)
	GEN_GP_FILE(gp_20000.c TrustedCoreFw/xmlstable TEE_Internal_API)
	GEN_GP_FILE(gp_30000.c DataStorage/xmlstable TEE_DataStorage_API)
	GEN_GP_FILE(gp_40000.c Time_Arithmetical/xmlstable TEE_TimeArithm_API)
	GEN_GP_FILE(gp_50000.c Crypto/xmlstable TEE_Crypto_API)
	set (GP_INCLUDES PRIVATE optee_test/host/xtest/gp/include)
endif()

if (CFG_PKCS11_TA)
	add_compile_options(-DCFG_PKCS11_TA)
	list (APPEND SRC optee_test/host/xtest/pkcs11_1000.c)
endif()

################################################################################
# Built binary
################################################################################
add_executable (${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME}
	PRIVATE optee_test/host/xtest
	PRIVATE optee_test/host/supp_plugin/include
	PRIVATE optee_test/host/xtest/adbg/include
	PRIVATE ${CMAKE_SOURCE_DIR}/optee-client/optee_client/libteec/include
	PRIVATE optee_os/lib/libutee/include
	PRIVATE optee_os/lib/libutils/ext/include
	PRIVATE optee_os/core/include
	PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
	${GP_INCLUDES}
)

target_link_libraries (${PROJECT_NAME}
	PRIVATE m
	PRIVATE xtest-ta-headers
	PRIVATE ckqteec
	PRIVATE ${MINK_LIBS}
	PRIVATE ${CMAKE_THREAD_LIBS_INIT}
	PRIVATE ${OPENSSL_PRIVATE_LINK}
)

################################################################################
# Install targets
################################################################################
install (TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
