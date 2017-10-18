cmake_minimum_required(VERSION 3.5)
project(IPCamTenvis)

set(CMAKE_C_STANDARD 90)

set(GCC_STANDARD $ENV{STANDARD})

#PRESTO_PLATFORM STREQUAL "ubuntu"
message(STATUS "Compiling for the X86")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_STANDARD} -O0 -g -Wall")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
set(PROXY_LIBS_PATH ${PROJECT_SOURCE_DIR}/../../Presto_new/lib)
set(REQUIRED_LIBRARIES
        m
        pthread
        curl
)

#all include paths
include_directories(
        ./
        ag_config
        ag_threads
        ${PROXY_LIBS_PATH}/pu_logger
        ${PROXY_LIBS_PATH}/presto_commands
        ${PROXY_LIBS_PATH}/pc_config
        ${PROXY_LIBS_PATH}/libhttpcomm
        ${PROXY_LIBS_PATH}/json
)

#Agent
file (GLOB SOURCE_FILES
        main.c
        ag_config/*.c
        ag_threads/*.c
        ${PROXY_LIBS_PATH}/pc_config/pc_config.c
        ${PROXY_LIBS_PATH}/presto_commands/*.c
        ${PROXY_LIBS_PATH}/pu_logger/*.c
        ${PROXY_LIBS_PATH}/json/*.c
)

add_executable(Tenvis ${SOURCE_FILES})

target_link_libraries(Tenvis ${REQUIRED_LIBRARIES})