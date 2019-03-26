

#Add sources to the project
set(SOURCES_PREFIX ${CMAKE_SOURCE_DIR}/src)
add_subdirectory(src)
list(APPEND SOS_SOURCELIST ${SOURCES})

set(SOS_CONFIG release)

# Kernel libs for standard arch's
set(SOS_OPTION kernel)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib-std.cmake)

#for linking to applications for standard arch's
set(SOS_BUILD_FLAGS -mlong-calls)
set(SOS_OPTION "")
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib-std.cmake)

