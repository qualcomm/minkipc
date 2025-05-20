cmake_minimum_required(VERSION 3.4)
project(minkipc)

option(OFFTARGET "Compile outside of build system" FALSE)
if (OFFTARGET)
	# Treat warnings as error
	string(APPEND CMAKE_C_FLAGS " -Werror")
	# Enable UBSan and immediately return when error is encountered
	string(APPEND CMAKE_C_FLAGS " -fsanitize=undefined -fno-sanitize-recover=all")
	# Enable ASan
	string(APPEND CMAKE_C_FLAGS " -fsanitize=address")
endif()

add_subdirectory(mink)
add_subdirectory(transport/libminksocket)

# Build Mink TEEC library
option(BUILD_MINKTEEC "Build MinkTEEC" TRUE)

option(BUILD_UNITTEST "Build unittest" FALSE)

# Build Mink ADAPTOR library
option(BUILD_MINKADAPTOR "Build MinkADAPTOR" TRUE)

include(GNUInstallDirs)

add_compile_options(
	-Wall -Wextra -Werror -Wshadow -Wcast-align
	-Wmissing-declarations -Wformat-security -Wmissing-noreturn
	-Wdeprecated -fPIC
)

if (BUILD_MINKADAPTOR)
	add_subdirectory(libminkadaptor)
endif()

if (BUILD_MINKTEEC)
	add_subdirectory(libminkteec)
endif()

enable_testing()
