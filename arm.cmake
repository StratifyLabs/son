

#Add sources to the project
set(SOURCES_PREFIX ${CMAKE_SOURCE_DIR}/src CACHE INTERNAL "src prefix")
add_subdirectory(src)
list(APPEND SOS_LIB_SOURCELIST ${SOURCES})

set(SOS_LIB_ARCH armv7-m CACHE INTERNAL "sos build ")
set(SOS_LIB_CONFIG kernel CACHE INTERNAL "sos build config release")
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)

set(SOS_LIB_ARCH armv7e-m CACHE INTERNAL "sos build ")
set(SOS_LIB_CONFIG kernel CACHE INTERNAL "sos build config release")
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)

#for linking to applications
set(SOS_LIB_BUILD_FLAGS -mlong-calls CACHE INTERNAL "sos lib build flags")
set(SOS_LIB_ARCH armv7-m CACHE INTERNAL "sos build ")
set(SOS_LIB_CONFIG release CACHE INTERNAL "sos build config release")
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)

set(SOS_LIB_ARCH armv7e-m CACHE INTERNAL "sos build ")
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)
