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
#include <netdb.h>

#include "pu_queue.h"
#include "pu_logger.h"

#include "ab_ring_bufer.h"
#include "at_cam_video_read.h"
#include "at_cam_video_write.h"
#include "ag_settings.h"
#include"ac_rtsp.h"
#include "ao_cma_cam.h"

#include "at_video_connector.h"


/*************************************************************************
 * Local data & functione
 */
#define AT_THREAD_NAME "VIDEO_CONNECTOR"

typedef enum {
    AT_STATE_UNDEF,
    AT_STATE_CONNECT,
    AT_STATE_AUTH,
    AT_STATE_SETUP,
    AT_STATE_START_PLAY,
    AT_STATE_PLAYING,
    AT_STATE_STOP_PLAY,
    AT_STATE_ON_ERROR
} t_at_states;

static pthread_t id;
static pthread_attr_t attr;

static volatile int stop = 1;       /* Thread stop flag */

/***********
 * Local connection data. To be replaced later
*/

static char video_host[5000];
static int video_port = 1935;
static char vs_session_id[100];

static const int fake_client_port = 11038;

/*************************************************/
static void shutdown_proc() {
    ac_close_session(AC_WOWZA);
    ac_close_session(AC_CAMERA);
    ac_rtsp_down();
    ab_close();         /* Erase videostream buffer */
}
static int init_proc() {
    /* Setup the ring buffer for video streaming */
    if(!ab_init(ag_getVideoChunksAmount())) {
        pu_log(LL_ERROR, "%s: Videostreaming buffer allocation error");
        return 0;
    }
    if(!ac_rtsp_init()) return 0;
    char url[4000] = {0};
    if(!ac_open_session(AC_WOWZA, ac_makeVSURL(url, sizeof(url), video_host, video_port, vs_session_id))) goto on_error;
    if(!ac_open_session(AC_CAMERA, ac_makeCamURL(url, sizeof(url), ag_getCamIP(), ag_getCamPort(), ag_getCamLogin(), ag_getCamPassword(), ag_getCamResolution()))) goto on_error;
    return 1;
    on_error:
    shutdown_proc();
    return 0;
}

static int start_streaming() {
    if(at_is_video_read_run()) at_stop_video_read();
    if(!at_start_video_read()) return -1;

    if(at_is_video_write_run()) at_stop_video_write();
    if(!at_start_video_write()) return -1;

    return 1;
}
static void stop_streaming() {
    at_set_stop_video_read();
    at_set_stop_video_write();

    if(at_is_video_read_run()) at_stop_video_read();
    if(at_is_video_write_run()) at_stop_video_write();
}

static void say_disconnected_to_vm() {
    pu_log(LL_INFO, "%s: %s", AT_THREAD_NAME, __FUNCTION__);
}

static void rtsp_logging(const char* who, const char* h, const char* b) {
    if(strlen(h)) pu_log(LL_DEBUG, "%s: Head = %s", who, h);
    if(strlen(b)) pu_log(LL_DEBUG, "%s: Body = %s", who, b);
}

static t_at_states process_connect() {
    char head[5000] = {0};
    char body[5000] = {0};
    int rc;

    rc = ac_req_options(AC_CAMERA, head, sizeof(head), body, sizeof(body));
    rtsp_logging("CAM-OPTIONS", head, body);
    if(!rc) return AT_STATE_ON_ERROR;

    rc = ac_req_options(AC_WOWZA, head, sizeof(head), body, sizeof(body));
    rtsp_logging("VS-OPTIONS", head, body);
    if(!rc) return AT_STATE_ON_ERROR;

    return AT_STATE_AUTH;
}
static t_at_states process_auth() {
    char head[5000] = {0};
    char body[5000] = {0};
    int rc;

    rc = ac_req_cam_describe(head, sizeof(head), body, sizeof(body));
    rtsp_logging("CAM-DESCRIBE", head, body);
    if(!rc) return AT_STATE_ON_ERROR;

    rc = ac_req_vs_announce1(body, head, sizeof(head), body, sizeof(body));
    rtsp_logging("VS-ANNOUNCE", head, body);
    if(!rc) return AT_STATE_ON_ERROR;

    rc = ac_req_vs_announce2(head, sizeof(head), body, sizeof(body));
    rtsp_logging("VS-AUTH", head, body);
    if(!rc) return AT_STATE_ON_ERROR;

    return AT_STATE_SETUP;
}
static t_at_states process_setup() {    /* NB! Video stream only!*/
    char head[5000] = {0};
    char body[5000] = {0};
    int rc, port;

    rc = ac_req_setup(AC_CAMERA, head, sizeof(head), body, sizeof(body), fake_client_port);
    rtsp_logging("CAM-SETUP", head, body);
    if(!rc) return AT_STATE_ON_ERROR;

    if(port = ao_get_server_port(head), port < 0) {
        pu_log(LL_ERROR, "%s: Get CAM UDP port error", AT_THREAD_NAME);
        return AT_STATE_ON_ERROR;
    }
    ag_saveServerPort(port);

    rc = ac_req_setup(AC_WOWZA, head, sizeof(head), body, sizeof(body), fake_client_port);
    rtsp_logging("VS-SETUP", head, body);
    if(!rc) return AT_STATE_ON_ERROR;

    if(port = ao_get_server_port(head), port < 0) {
        pu_log(LL_ERROR, "%s: Get VS UDP port error", AT_THREAD_NAME);
        return AT_STATE_ON_ERROR;
    }
    ag_saveClientPort(port);

    return AT_STATE_START_PLAY;
}
static t_at_states process_play() {
    char head[5000] = {0};
    char body[5000] = {0};
    int rc;

    rc = ac_req_play(AC_CAMERA, head, sizeof(head), body, sizeof(body));
    rtsp_logging("CAM-PLAY", head, body);
    if(!rc) return AT_STATE_ON_ERROR;

    rc = ac_req_play(AC_WOWZA, head, sizeof(head), body, sizeof(body));
    rtsp_logging("VS-PLAY", head, body);
    if(!rc) return AT_STATE_ON_ERROR;

    if(!start_streaming()) return AT_STATE_ON_ERROR;

    return AT_STATE_PLAYING;
}
static void process_stop() {
    char head[5000] = {0};
    char body[5000] = {0};

    ac_req_teardown(AC_CAMERA, head, sizeof(head), body, sizeof(body));
    rtsp_logging("CAM-TEARDOWN", head, body);

    ac_req_teardown(AC_WOWZA, head, sizeof(head), body, sizeof(body));
    rtsp_logging("VS-TEARDOWN", head, body);

    stop_streaming();
}
static void* vc_thread(void* params) {
    int tear_down;
    t_at_states state;

on_reconnect:
    if(!init_proc()) {
        pu_log(LL_ERROR, "%s exiting by hard error", AT_THREAD_NAME);
        say_disconnected_to_vm();
        pthread_exit(NULL);
    }
    tear_down = 0;
    state = AT_STATE_CONNECT;

    while(!stop) {
        switch(state) {
            case AT_STATE_CONNECT:
                state = process_connect();
                break;
            case AT_STATE_AUTH:
                state = process_auth();
                break;
            case AT_STATE_SETUP:
                state = process_setup(); /* Save VS & CAM UDP ports */
                break;
            case AT_STATE_START_PLAY:
                state = process_play();
                break;
            case AT_STATE_PLAYING:
                sleep(1);
                break;
            case AT_STATE_ON_ERROR:
                stop = 1;
                break;
            default:
                pu_log(LL_ERROR, "%s: Unknown state = %d. Exiting", AT_THREAD_NAME, state);
                state = AT_STATE_ON_ERROR;
                break;
        }
    }
    switch (state) {
        case AT_STATE_ON_ERROR:
            pu_log(LL_ERROR, "%s: Exit by error. Reconnect", AT_THREAD_NAME);
            break;
        case AT_STATE_PLAYING:
            pu_log(LL_INFO, "%s: Exit by stop playing", AT_THREAD_NAME);
            process_stop();
            tear_down = 1;
            break;
        default:
            pu_log(LL_ERROR, "%s: Exit by request", AT_THREAD_NAME);
            tear_down = 1;
            break;
    }
    stop_streaming();
    if(!tear_down) {
        pu_log(LL_ERROR, "%s: Reconnect due to connection problems", AT_THREAD_NAME);
        goto on_reconnect;
    }
    shutdown_proc();
    pthread_exit(NULL);
}

/*************************************************************************
 * Global functions definition
 */
int at_start_video_connector(const char* host, int port, const char* session_id) {
    if(is_video_connector_run()) {
        pu_log(LL_WARNING, "%s: Vidoe connector already run. Start ignored", __FUNCTION__);
        return 1;
    }

    pu_log(LL_DEBUG, "%s: VS connection parameters: host = %s, port = %d, vs_session_id = %s", __FUNCTION__, host, video_port, vs_session_id);
    strncpy(video_host, host, sizeof(video_host));
    strncpy(vs_session_id, session_id, sizeof(vs_session_id));

    struct hostent* hn = gethostbyname(host);
    if(!hn) {
        pu_log(LL_ERROR, "%s: Can't get IP of VS server: %d - %s", __FUNCTION__, h_errno, strerror(h_errno));
        return -1;
    }
    ag_saveClientIP(hn->h_addr);
    stop = 0;

    if(pthread_attr_init(&attr)) {stop = 1; return 0;}
    if(pthread_create(&id, &attr, &vc_thread, NULL)) {stop = 1; return 0;}


    return 1;
}
int at_stop_video_connector() {
    void* ret;
    if(!is_video_connector_run()) {
        pu_log(LL_WARNING, "%s - %s: The tread already stop. Stop ignored", AT_THREAD_NAME, __FUNCTION__);
        return 1;
    }

    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    return 1;
}

int is_video_connector_run() {
    return !stop;
}





