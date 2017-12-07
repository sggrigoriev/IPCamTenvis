cmake_minimum_required(VERSION 3.5)
project(IPCamTenvis)

set(CMAKE_C_STANDARD 90)

set(GCC_STANDARD $ENV{STANDARD})

#add_definitions( -DPROXY_SEPARATE_RUN )     #Means w/o WUD
add_definitions( -DPPC_LOCAL_TEST)

#PRESTO_PLATFORM STREQUAL "ubuntu"
message(STATUS "Compiling for the X86")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_STANDARD} -O0 -g -Wall")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
set(PROXY_LIBS_PATH ${PROJECT_SOURCE_DIR}/../../../Presto_new/lib)
set(REQUIRED_LIBRARIES
        m
        pthread
        curl
)

#all include paths
include_directories(
        ./
        ag_cam_io
        ag_converter
        ag_config
        ag_threads
        ag_queues
        ag_ring_buffer
        ${PROXY_LIBS_PATH}/pu_logger
        ${PROXY_LIBS_PATH}/presto_commands
        ${PROXY_LIBS_PATH}/pc_config
        ${PROXY_LIBS_PATH}/pu_queues
        ${PROXY_LIBS_PATH}/libtimer
        ${PROXY_LIBS_PATH}/libhttpcomm
        ${PROXY_LIBS_PATH}/libtcp
        ${PROXY_LIBS_PATH}/json
)

#Agent
file (GLOB SOURCE_FILES
        main.c
        ag_cam_io/*.c
        ag_converter/*.c
        ag_config/*.c
        ag_threads/*.c
        ag_queues/*.c
        ag_ring_buffer/ab_ring_bufer.c
        ${PROXY_LIBS_PATH}/pc_config/pc_config.c
        ${PROXY_LIBS_PATH}/presto_commands/*.c
        ${PROXY_LIBS_PATH}/pu_queues/*.c
        ${PROXY_LIBS_PATH}/pu_logger/*.c
        ${PROXY_LIBS_PATH}/libtimer/*.c
        ${PROXY_LIBS_PATH}/libtcp/*.c
        ${PROXY_LIBS_PATH}/json/*.c
)

#ab_test
file (GLOB SOURCE_FILES_AB_TEST
        ag_ring_buffer/*.c
        ${PROXY_LIBS_PATH}/pu_logger/*.c
)

#local_test
file (GLOB SOURCE_FILES_LOCAL_TEST
        vlc_local_test/*.c
        ag_cam_io/*.c
        ag_converter/*.c
        ag_config/*.c
        ag_threads/at_cam_video_read.c
        ag_threads/at_cam_video_write.c
        ag_threads/at_video_connector.c
        ag_queues/*.c
        ag_ring_buffer/ab_ring_bufer.c
        ${PROXY_LIBS_PATH}/pc_config/pc_config.c
        ${PROXY_LIBS_PATH}/presto_commands/*.c
        ${PROXY_LIBS_PATH}/pu_queues/*.c
        ${PROXY_LIBS_PATH}/pu_logger/*.c
        ${PROXY_LIBS_PATH}/libtimer/*.c
        ${PROXY_LIBS_PATH}/libtcp/*.c
        ${PROXY_LIBS_PATH}/json/*.c
)

add_executable(Tenvis ${SOURCE_FILES})
target_link_libraries(Tenvis ${REQUIRED_LIBRARIES})

add_executable(ab_test ${SOURCE_FILES_AB_TEST})
target_link_libraries(ab_test ${REQUIRED_LIBRARIES})

add_executable(local_test ${SOURCE_FILES_LOCAL_TEST})
target_link_libraries(local_test ${REQUIRED_LIBRARIES})
