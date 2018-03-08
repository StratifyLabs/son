

#Add sources to the project
set(SOURCES_PREFIX ${CMAKE_SOURCE_DIR}/src)
add_subdirectory(src)
list(APPEND SOS_LIB_SOURCELIST ${SOURCES})

set(SOS_LIB_TYPE release)

# Kernel libs for standard arch's
set(SOS_LIB_OPTION kernel)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib-std.cmake)

#for linking to applications for standard arch's
set(SOS_LIB_BUILD_FLAGS -mlong-calls)
set(SOS_LIB_OPTION "")
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib-std.cmake)

