cmake_minimum_required(VERSION 3.12)
project(Libs)

set(CMAKE_CXX_STANDARD 98)

set(AGENT_PLATFORM $ENV{PLATFORM})
set(PP_SDK_PATH $ENV{SDK_PATH})
set(GCC_STANDARD $ENV{STANDARD})

if(AGENT_PLATFORM STREQUAL "ubuntu")

add_definitions(-D__LINUX__)
add_definitions(-DEXTERN=)

include_directories(
        ${CMAKE_SOURCE_DIR}/ipcam_inc
)

file(GLOB SOURCE_FILES
        ipcam_src/*.cpp
        ipcam_src/*.c
)

endif()

if(AGENT_PLATFORM STREQUAL "ubuntu")

add_library(ipcam STATIC ${SOURCE_FILES})

endif()
