project(librpmbservice
	VERSION 1.0.0
	LANGUAGES C
)

add_compile_options(
	-Wstrict-prototypes -Wmissing-prototypes -Wbad-function-cast
)

# Uncomment for debug console output instead of syslog
# add_definitions(-DRPMB_DEBUG_CONSOLE_OUTPUT)

set(SRC
	../listenercbo/src/CListenerCBO.c
	../listenercbo/src/memscpy.h
	rpmb_service.c
	rpmb.c
	rpmb_emmc.c
	rpmb_ufs.c
	rpmb_logging.c
)

add_library(rpmbservice SHARED ${SRC})

set_target_properties(rpmbservice PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
)

add_dependencies(rpmbservice listenercbo_MINKHEADERS)

target_include_directories(rpmbservice
	PRIVATE ../listenercbo/include
	PRIVATE ../listenercbo/idl
	PRIVATE ${CMAKE_SOURCE_DIR}/libminkadaptor/include
)

target_link_libraries(rpmbservice
	PRIVATE minkadaptor
)

install(TARGETS rpmbservice
	LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}"
)
