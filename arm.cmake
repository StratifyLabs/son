

#Add sources to the project
set(SOURCES_PREFIX ${CMAKE_SOURCE_DIR}/src)
add_subdirectory(src)
list(APPEND SOS_LIB_SOURCELIST ${SOURCES})

set(SOS_LIB_TYPE release)

# Kernel libs for armv7-m and armv7e-m
set(SOS_LIB_ARCH armv7-m)
set(SOS_LIB_OPTION kernel)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)

set(SOS_LIB_ARCH armv7e-m)
set(SOS_LIB_OPTION kernel)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)

#for linking to applications for armv7-m and armv7e-m
set(SOS_LIB_BUILD_FLAGS -mlong-calls)
set(SOS_LIB_ARCH armv7-m)
set(SOS_LIB_OPTION "")
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)

set(SOS_LIB_ARCH armv7e-m)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)
