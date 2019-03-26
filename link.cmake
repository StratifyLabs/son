

#Add sources to the project
set(SOURCES_PREFIX ${CMAKE_SOURCE_DIR}/src)
add_subdirectory(src)
list(APPEND SOS_SOURCELIST ${SOURCES})

set(SOS_CONFIG release)
set(SOS_OPTION "")
set(SOS_ARCH link)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)
set(SOS_OPTION link)
set(SOS_ARCH link)
include(${SOS_TOOLCHAIN_CMAKE_PATH}/sos-lib.cmake)
