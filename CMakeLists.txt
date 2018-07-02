# **********************************************
# DO NOT FORGET TO DEFINE ENVIRONMENT VARIABLES
# - SDK_PATH
# - PLATFORM
# **********************************************

cmake_minimum_required(VERSION 3.5)
project(IPCamTenvis C)

set(AGENT_PLATFORM $ENV{PLATFORM})
set(PP_SDK_PATH $ENV{SDK_PATH})
set(GCC_STANDARD $ENV{STANDARD})

set(PROXY_LIBS_PATH ${PP_SDK_PATH}/lib)

message("path = $ENV{SDK_PATH}")

if(AGENT_PLATFORM STREQUAL "ubuntu")

    #add_definitions(-DNOPOLL_TRACE)            # Enabling nopoll ()Web Socket connection) logging to stdout
    #add_definitions( -DCURL_TRACE )            # Switches on CURL trace for AlfaPro RTSP session
    #add_definitions( -DRW_CYCLES=999 )        # will stop RW thread after RW_CYCLES iterations --  helpful for stress-tests

    if($ENV{NO_GST_PLAGINS} STREQUAL "1")        # To separate build for env with gst plugins installed
        add_definitions( -DGST_EXT )
    endif()

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(gst REQUIRED gstreamer-1.0 gstreamer-rtsp-1.0 glib-2.0 gobject-2.0)

#Print variables
    message("NO_GST_PLAGINS = $ENV{NO_GST_PLAGINS}")
    message("gst_FOUND = ${gst_FOUND}")
    message("gst_LIBRARIES = ${gst_LIBRARIES}")
    message("gst_INCLUDE_DIRS = ${gst_INCLUDE_DIRS}")
    message("PROXY_LIBS_PATH = ${PROXY_LIBS_PATH}")

    message(STATUS "Compiling for ubuntu")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_STANDARD} -O0 -g -Wall")

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
    set(REQUIRED_LIBRARIES
        ${gst_LIBRARIES}
        curl
        m
        pthread
        nopoll
        ssl
        crypto
    )
    include_directories(
        ./
        ag_cam_io
        ag_converter
        ag_config
        ag_threads
        ag_queues
        ag_ring_buffer
        au_string
        gsrtsp_local
        ${PROXY_LIBS_PATH}/pu_logger
        ${PROXY_LIBS_PATH}/presto_commands
        ${PROXY_LIBS_PATH}/pc_config
        ${PROXY_LIBS_PATH}/pu_queues
        ${PROXY_LIBS_PATH}/libtimer
        ${PROXY_LIBS_PATH}/libhttpcomm
        ${PROXY_LIBS_PATH}/libtcp
        ${PROXY_LIBS_PATH}/json
        ${gst_INCLUDE_DIRS}
        /usr/local/include/nopoll
    )
elseif(AGENT_PLATFORM STREQUAL "hisilicon")
    add_definitions( -DGST_EXT)

    file(MAKE_DIRECTORY ../build_tenvis/${AGENT_PLATFORM})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../build_tenvis/${AGENT_PLATFORM})

#    set(CMAKE_VERBOSE_MAKEFILE ON)
    set(CMAKE_SYSTEM_NAME Linux)
    set(TOOLCHAIN_PATH $ENV{TOOLCHAIN})
    set(SYSROOT_PATH $ENV{SYSROOT})
    set(COMPILER_PREFIX arm-hisiv500-linux-uclibcgnueabi-)
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PATH}/bin/${COMPILER_PREFIX}gcc)
#    set(CMAKE_SYSROOT ${TOOLCHAIN_PATH}/target/usr)
    set(CMAKE_FORCE_C_COMPILER ${TOOLCHAIN_PATH}/bin/${COMPILER_PREFIX}gcc GNU)
#    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
#    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
#    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
#    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${SYSROOT_PATH}/usr/include/gstreamer-1.0 -I${SYSROOT_PATH}/usr/include/glib-2.0 -I${SYSROOT_PATH}/usr/include -D_BSD_SOURCE -L${SYSROOT_PATH}/usr/lib -D_GNU_SOURCE -O3 -Wall -s")

    set(REQUIRED_LIBRARIES
            curl
            m
            pthread
            nopoll
            ssl
            crypto)


    include_directories(
        ./
        ag_cam_io
        ag_converter
        ag_config
        ag_threads
        ag_queues
        ag_ring_buffer
        au_string
        gsrtsp_local
        ${PROXY_LIBS_PATH}/proxy_eui64
        ${PROXY_LIBS_PATH}/pu_logger
        ${PROXY_LIBS_PATH}/presto_commands
        ${PROXY_LIBS_PATH}/pc_config
        ${PROXY_LIBS_PATH}/pu_queues
        ${PROXY_LIBS_PATH}/libtimer
        ${PROXY_LIBS_PATH}/libhttpcomm
        ${PROXY_LIBS_PATH}/libtcp
        ${PROXY_LIBS_PATH}/json
        ${SYSROOT_PATH}/usr/include
        #        ${gst_INCLUDE_DIRS}
        #        /usr/local/include/nopoll
    )
    message(STATUS "Compiling for hiSilicon CPU (${TOOLCHAIN_PATH}/target/usr)")
    message(STATUS "sysroot is (${SYSROOT_PATH})")
endif()

#Agent
file (GLOB SOURCE_FILES
        main.c
        ag_cam_io/*.c
        ag_converter/*.c
        ag_config/*.c
        ag_threads/*.c
        ag_queues/*.c
        ag_ring_buffer/ab_ring_bufer.c
        au_string/*.c
        gsrtsp_local/*.c
        ${PROXY_LIBS_PATH}/pc_config/pc_config.c
        ${PROXY_LIBS_PATH}/presto_commands/*.c
        ${PROXY_LIBS_PATH}/pu_queues/*.c
        ${PROXY_LIBS_PATH}/pu_logger/*.c
        ${PROXY_LIBS_PATH}/libtimer/*.c
        ${PROXY_LIBS_PATH}/libtcp/*.c
        ${PROXY_LIBS_PATH}/json/*.c
)

if(AGENT_PLATFORM STREQUAL "ubuntu")

    add_executable(Tenvis ${SOURCE_FILES})
    target_link_libraries(Tenvis ${REQUIRED_LIBRARIES})

elseif(AGENT_PLATFORM STREQUAL "hisilicon")
            add_executable(Tenvis ${SOURCE_FILES})
            target_link_libraries(Tenvis ${REQUIRED_LIBRARIES}

            ${SYSROOT_PATH}/usr/lib/libgstapp-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstaudio-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstbase-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstcheck-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstcontroller-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstfft-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstnet-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstpbutils-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstreamer-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstriff-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstrtp-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstrtsp-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstsdp-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgsttag-1.0.a
            ${SYSROOT_PATH}/usr/lib/libgstvideo-1.0.a

            ${SYSROOT_PATH}/usr/lib/libglib-2.0.a
            ${SYSROOT_PATH}/usr/lib/libgio-2.0.a
            ${SYSROOT_PATH}/usr/lib/libglib-2.0.a
            ${SYSROOT_PATH}/usr/lib/libgobject-2.0.a
            ${SYSROOT_PATH}/usr/lib/libglib-2.0.a
            ${SYSROOT_PATH}/usr/lib/libgettextlib.a
            ${SYSROOT_PATH}/usr/lib/libgmodule-2.0.a
            ${SYSROOT_PATH}/usr/lib/libglib-2.0.a
            ${SYSROOT_PATH}/usr/lib/libz.a
            ${SYSROOT_PATH}/usr/lib/libgettextlib.a

            ${SYSROOT_PATH}/usr/lib/libintl.a
            ${SYSROOT_PATH}/usr/lib/libffi.a
            ${SYSROOT_PATH}/usr/lib/libcurl.a
            ${SYSROOT_PATH}/usr/lib/libdl.so.0

            )
endif()