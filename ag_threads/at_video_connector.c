/*
 *  Copyright 2017 People Power Company
 *
 *  This code was developed with funding from People Power Company
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
*/
/*
 Created by gsg on 23/11/17.
*/
#include <pthread.h>
#include <memory.h>
#include <errno.h>
#include <ag_converter/ao_cma_cam.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ag_cam_io/ac_rtsp.h>

#include "pu_logger.h"
#include "pu_queue.h"
#include "lib_tcp.h"

#include "ab_ring_bufer.h"
#include "ao_cma_cam.h"
#include "ac_tcp.h"
#include "at_cam_video_read.h"
#include "at_cam_video_write.h"
#include "ag_settings.h"

#include "at_video_connector.h"


/*************************************************************************
 * Local data & functione
 */
#define AT_THREAD_NAME "VIDEO_CONNECTOR"

static pthread_t id;
static pthread_attr_t attr;

static volatile int stop;       /* Thread stop flag */

static int init_proc();
static void shutdown_proc();

#ifndef LOCAL_TEST
    static void* vc_thread(void* params);
#elif defined(LOCAL_TEST) && !defined(PPC_LOCAL_TEST)
    static int vlc_connect();
    static void vlc_disconnect(int write_socket);
#endif
#ifdef PPC_LOCAL_TEST
    static char* ppc_ip = "ххх.ххх..."; /* Set IP before use! */
    static int ppc_port = -1;           /* Set port before use! */

    static int ppc_connect();
    static void ppc_disconnect(int write_socket);
#endif
static const char* remote_ip(int sock, char* buf, size_t size);

static void stop_streaming();
static int video_server_connect();
static void video_server_diconnect(int write_socket);
static int cam_connect();
static void cam_disconnect(int sock);

static void say_cant_connect_to_vm();
static void say_connected_to_vm();
static void say_disconnected_to_vm();

/*************************************************************************
 * Global functions definition
 */
int at_start_video_connector() {
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &vc_thread, NULL)) return 0;
    return 1;
}
int at_stop_video_connector() {
    void* ret;

    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    return 1;
}

/*************************************************************************
 * Local functionsimplementation
 */
#ifndef LOCAL_TEST
static
#endif
void* vc_thread(void* params) {
    int video_sock;
    int cam_sock;

    int tear_down;
    char msg[LIB_HTTP_MAX_MSG_SIZE] = {0};
    char cam_uri[LIB_HTTP_MAX_URL_SIZE] = {0};

    stop = 0;

    if(!init_proc()) {
        pu_log(LL_ERROR, "%s exiting by hard error", AT_THREAD_NAME);
#ifndef LOCAL_TEST
        pthread_exit(NULL);
#else
        return NULL;
#endif
    }

    on_reconnect:
        video_sock = -1;
        cam_sock = -1;
        tear_down = 0;

        ao_makeURI(cam_uri, sizeof(cam_uri), ag_getCamIP(), ag_getCamPort(), ag_getCamLogin(), ag_getCamPassword(), ag_getCamResolution());

        if((video_sock = video_server_connect()) < 0) {
            say_cant_connect_to_vm();
            goto on_error;
        }
        if((cam_sock = cam_connect()) < 0) {
            say_cant_connect_to_vm();
            goto on_error;
        }
        say_connected_to_vm();
    while(!stop) {
        t_ac_rtsp_msg data;

        if(!ac_tcp_read(video_sock, msg, sizeof(msg), stop)) goto on_error;

        data.msg_type = ao_get_msg_type(msg);
        data.number = ao_get_msg_number(msg);
        ao_get_uri(data.uri, sizeof(data.uri), msg);
        ao_cam_replace_uri(msg, sizeof(msg), cam_uri);

        pu_log(LL_DEBUG, "%s: Request to cam: %s", AT_THREAD_NAME, msg);

        if(!ac_tcp_write(cam_sock, msg, stop)) goto on_error;
        if(!ac_tcp_read(cam_sock, msg, sizeof(msg), stop)) goto on_error;
if(data.msg_type == AC_PLAY) {
    pu_log(LL_DEBUG, "%s PLAY - answer",AT_THREAD_NAME);
}
        ao_cam_replace_uri(msg, sizeof(msg), data.uri);

        if(data.number != ao_get_msg_number(msg)) {
            pu_log(LL_ERROR, "%s: Cam answer got different CSeq! Request = %d, Response = %d in message %s", AT_THREAD_NAME, data.number, ao_get_msg_number(msg), msg);
            goto on_error;
        }
        switch(data.msg_type) {
            case AC_SETUP:
                if (data.b.setup.track_number == 0) {
                    ag_saveClientPort(ao_get_client_port(msg));
                    ag_saveServerPort(ao_get_server_port(msg));
                    if(!at_start_video_write()) goto on_error;
                    if(!at_start_video_read()) goto on_error;
                }
                break;
            case AC_PLAY:
                say_connected_to_vm();
                break;
            case AC_TEARDOWN:
                tear_down = 1;
                stop = 1;
                say_disconnected_to_vm();
                continue;
            case AC_UNDEFINED:
                pu_log(LL_WARNING, "%s: Unrecognized request from Video Server", AT_THREAD_NAME);
                break;
            default:
                break;
        }
        pu_log(LL_DEBUG, "%s: Answer to video server: %s", AT_THREAD_NAME, msg);
        if(!ac_tcp_write(video_sock, msg, stop)) goto on_error;
    }
    on_error:
    stop_streaming();
    video_server_diconnect(video_sock);
    video_sock = -1;

    cam_disconnect(cam_sock);
    cam_sock = -1;

    if(!tear_down) {
        pu_log(LL_ERROR, "%s: Reconnect due to connection problems", AT_THREAD_NAME);
        goto on_reconnect;
    }
    shutdown_proc();
#ifndef LOCAL_TEST
    pthread_exit(NULL);
#else
    return NULL;
#endif
}

static int init_proc() {
    /* Setup the ring buffer for video streaming */
    if(!ab_init(ag_getVideoChunksAmount())) {
        pu_log(LL_ERROR, "%s: Videostreaming buffer allocation error");
        return 0;
    }
    return 1;
}
static void shutdown_proc() {
    ab_close();         /* Erase videostream buffer */
}

static void stop_streaming() {
    at_set_stop_video_read();
    at_set_stop_video_write();

    if(at_is_video_read_run()) at_stop_video_read();
    if(at_is_video_write_run()) at_stop_video_write();
}

#ifdef LOCAL_TEST
    int server_socket = -1;
#endif

static int video_server_connect() {
#ifdef LOCAL_TEST
    #ifdef PPC_LOCAL_TEST
        return ppc_connect();
    #else
        return vlc_connect();
    #endif
#endif
/* Here starts the correct connection to vidoe server */
    return -1;
}
static void video_server_diconnect(int write_socket) {
#ifdef LOCAL_TEST
    #ifdef PPC_LOCAL_TEST
        ppc_disconnect(write_socket);
    #else
        vlc_disconnect(write_socket);
    #endif
#endif
}
static int cam_connect() {
    return ac_tcp_client_connect(ag_getCamIP(), ag_getCamPort());
}
static void cam_disconnect(int sock) {
    if(sock >= 0) close(sock);
}

#if defined(LOCAL_TEST) && !defined(PPC_LOCAL_TEST)
static int vlc_connect(){
    server_socket = lib_tcp_get_server_socket(DEFAULT_LOCAL_AGENT_PORT);
    if(server_socket < 0) {
        pu_log(LL_ERROR, "%s: Open server TCP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }

    int conn_socket = lib_tcp_listen(server_socket, 3600);
    if(conn_socket < 0) {
        close(server_socket);
        pu_log(LL_ERROR, "%s: Listen incoming TCP connection failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }
    char buf[INET6_ADDRSTRLEN+1];
    ag_saveClientIP(remote_ip(conn_socket, buf, sizeof(buf)));
    return conn_socket;

}
static void vlc_disconnect(int write_socket) {
    if(write_socket >=0) close(write_socket);
    if(server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
}
#endif

#ifdef PPC_LOCAL_TEST
static int ppc_connect() {
    int conn_socket;

    if(conn_socket = ac_tcp_client_connect(ppc_ip, ppc_port), conn_socket < 0) return -1;

    char buf[INET6_ADDRSTRLEN+1];
    ag_saveClientIP(remote_ip(conn_socket, buf, sizeof(buf)));
    return conn_socket;

}
static void ppc_disconnect(int write_socket) {
    if(write_socket >=0) close(write_socket);
    if(server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
}
#endif

static const char* remote_ip(int sock, char* buf, size_t size) {
    socklen_t len;
    struct sockaddr_storage addr;
//    int port;

    len = sizeof addr;

    getpeername(sock, (struct sockaddr*)&addr, &len);

    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
//    port = ntohs(s->sin_port);
    inet_ntop(AF_INET, &s->sin_addr, buf, size);
    return buf;
}

static void say_cant_connect_to_vm() {
    pu_log(LL_INFO, "%s: %s", AT_THREAD_NAME, __FUNCTION__);
}
static void say_connected_to_vm() {
    pu_log(LL_INFO, "%s: %s", AT_THREAD_NAME, __FUNCTION__);
}
static void say_disconnected_to_vm() {
    pu_log(LL_INFO, "%s: %s", AT_THREAD_NAME, __FUNCTION__);
}


