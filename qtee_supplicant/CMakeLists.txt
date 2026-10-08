project(qtee_supplicant C)

# ''Source files''.

set(SRC
	src/qtee_supplicant.c
	src/listener_mngr.c
	src/listener_mngr.h
)

# ''Built binary''.

add_executable(${PROJECT_NAME} ${SRC})

# ''Optional Flags''.

if(BUILD_TIME_LISTENER)
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE -DTIME_LISTENER)
endif()

if(BUILD_TA_AUTOLOAD_LISTENER)
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE -DTA_AUTOLOAD_LISTENER)
endif()

if(PLUGIN_DIR)
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE PLUGIN_PATH="${PLUGIN_DIR}/")
endif()

if(BUILD_FS_LISTENER)
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE -DFS_LISTENER)
endif()

if(BUILD_GPFS_LISTENER)
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE -DGPFS_LISTENER)
endif()

if(BUILD_RPMB_LISTENER)
	target_compile_definitions(${PROJECT_NAME}
		PRIVATE -DRPMB_LISTENER)
endif()

# ''Headers and dependencies''.

target_link_libraries(${PROJECT_NAME}
	PRIVATE dl
)

install(TARGETS ${PROJECT_NAME} DESTINATION "${CMAKE_INSTALL_BINDIR}")

configure_file(sfs_config.in sfs_config @ONLY)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/sfs_config" DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

if (DEFINED SYSTEMD_UNIT_DIR AND NOT "${SYSTEMD_UNIT_DIR}" STREQUAL "")
	configure_file(qteesupplicant.service.in qteesupplicant.service @ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qteesupplicant.service" DESTINATION "${SYSTEMD_UNIT_DIR}")
	configure_file(sfsconfig.service.in sfsconfig.service @ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sfsconfig.service" DESTINATION "${SYSTEMD_UNIT_DIR}")
endif()
if (DEFINED UDEV_DIR AND NOT "${UDEV_DIR}" STREQUAL "")
	configure_file(qcomtee-udev.rules.in 99-qcomtee-udev.rules @ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/99-qcomtee-udev.rules" DESTINATION ${UDEV_DIR})
endif()
