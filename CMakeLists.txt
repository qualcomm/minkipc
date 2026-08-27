cmake_minimum_required(VERSION 3.10)
project(minkipc)

# Build Mink TEEC library
option(BUILD_MINKTEEC "Build MinkTEEC" TRUE)

# Let MinkTEEC talk to libqcomtee directly instead of going through libminkadaptor
option(MINKTEEC_DIRECT_QCOMTEE "Build MinkTEEC directly on top of libqcomtee" FALSE)

option(BUILD_UNITTEST "Build unittest" FALSE)

# Build Time listener
option(BUILD_TIME_LISTENER "Build Time Listener" TRUE)
# Build TA Autoload listener
option(BUILD_TA_AUTOLOAD_LISTENER "Build TA autoload Listener" TRUE)
# Build File system listener
option(BUILD_FS_LISTENER "Build File system Listener" TRUE)
# Build Secure File system listener
option(BUILD_GPFS_LISTENER "Build GP File system Listener" TRUE)
# Use GLib for strlcat/strlcpy fallback
option(USE_GLIB "Use GLIB functions" FALSE)
# Use pkg-config for discovering install target directory
option(CFG_USE_PKGCONFIG "Use pkg-config for discovering install target directory for systemd and udev files." OFF)
# Build RPMB listener
option(BUILD_RPMB_LISTENER "Build RPMB Listener" TRUE)
# Build RPMB Client
option(BUILD_RPMB_CLIENT "Build RPMB Client" TRUE)

# Build PKCS-11
option(BUILD_PKCS11 "Build PKCS#11" TRUE)

include(GNUInstallDirs)

set(MINKIDLC_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/idlc" CACHE PATH "Directory containing the idlc executable")
set(SYSTEMD_UNIT_DIR "" CACHE PATH "Directory containing the systemd unit path")
set(UDEV_DIR "" CACHE PATH "Directory containing the UDEV path")
set(PERSIST_MOUNT_UNIT "" CACHE PATH "Name of the systemd unit responsible for mounting the persist partition")
set(DISTRO_USER "root" CACHE STRING "Distro specific user for systemd service and devices")
set(DISTRO_GROUP "root" CACHE STRING "Distro specific group for the systemd service devices")

if ("${MINKIDLC_BIN_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/idlc")
	file(DOWNLOAD
	https://github.com/quic/mink-idl-compiler/releases/download/v0.2.0/idlc
	${MINKIDLC_BIN_DIR}/idlc
	)
	file(CHMOD ${MINKIDLC_BIN_DIR}/idlc PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)
endif()

add_compile_options(
	-Wall -Wextra -Werror -Wshadow -Wcast-align
	-Wmissing-declarations -Wformat-security -Wmissing-noreturn
	-Wdeprecated -fPIC
)

add_subdirectory(libminkadaptor)

if (BUILD_MINKTEEC)
	add_subdirectory(libminkteec)
endif()

add_subdirectory(qtee_supplicant)
add_subdirectory(listeners/listenercbo)

if(BUILD_TIME_LISTENER)
	add_subdirectory(listeners/libtimeservice)
endif()

if(BUILD_TA_AUTOLOAD_LISTENER)
	add_subdirectory(listeners/libtaautoload)
endif()

if(BUILD_FS_LISTENER OR BUILD_GPFS_LISTENER)
	add_subdirectory(listeners/libfsservice)
endif()

if(BUILD_RPMB_LISTENER)
	add_subdirectory(listeners/librpmbservice)
endif()

if (BUILD_PKCS11)
	add_subdirectory(optee-client)
	if (BUILD_UNITTEST)
		add_subdirectory(optee-test)
	endif()
endif()

if(BUILD_RPMB_CLIENT)
	add_subdirectory(rpmb_client)
endif()
