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
#include <arpa/inet.h>

#include "pu_logger.h"

#include "ab_ring_bufer.h"
#include "at_cam_video_read.h"
#include "at_cam_video_write.h"
#include "ag_settings.h"
#include "ac_rtsp.h"
#include "au_string.h"
#include "ac_wowza.h"
#include "ac_alfapro.h"

#include "at_video_connector.h"

#define AT_THREAD_NAME "VIDEO_CONNECTOR"

/*************************************************************************
 * Local data & functione
 */

t_at_rtsp_session* CAM_SESSION = NULL;
t_at_rtsp_session* PLAYER_SESSION = NULL;

struct {
    char host[LIB_HTTP_MAX_URL_SIZE];
    int port;
    char session_id[100];
} IN_PARAMS;



static pthread_t id;
static pthread_attr_t attr;

static volatile int stop = 1;       /* Thread stop flag */

/*************************************************/
static void shutdown_proc() {
    ac_close_session(PLAYER_SESSION);
    ac_close_session(CAM_SESSION);
    ac_rtsp_down(PLAYER_SESSION);
    ac_rtsp_down(CAM_SESSION);
    ab_close();         /* Erase videostream buffer */
}
static int init_proc() {
    /* Setup the ring buffer for video streaming */
    if(!ab_init(ag_getVideoChunksAmount())) {
        pu_log(LL_ERROR, "%s: Videostreaming buffer allocation error");
        return 0;
    }
    if(CAM_SESSION = ac_rtsp_init(AC_CAMERA), CAM_SESSION == NULL) {
        ab_close();
        return 0;
    }
    if(PLAYER_SESSION = ac_rtsp_init(AC_WOWZA), PLAYER_SESSION == NULL) {
        ab_close();
        ac_rtsp_down(CAM_SESSION);
        return 0;
    }

    char url[LIB_HTTP_MAX_URL_SIZE];

    if(!ac_open_session(PLAYER_SESSION, ac_make_wowza_url(url, sizeof(url), IN_PARAMS.host, IN_PARAMS.port, IN_PARAMS.session_id), IN_PARAMS.session_id)) goto on_error;
    if(!ac_open_session(CAM_SESSION, ac_makeAlfaProURL(url, sizeof(url), ag_getCamIP(), ag_getCamPort(), ag_getCamLogin(), ag_getCamPassword(), ag_getCamResolution()), "")) goto on_error;

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

static t_ac_rtsp_states process_connect() {

    if(!ac_req_options(CAM_SESSION)) return AC_STATE_ON_ERROR;
    if(!ac_req_options(PLAYER_SESSION)) return AC_STATE_ON_ERROR;

    return AC_STATE_DESCRIBE;
}
static t_ac_rtsp_states process_describe() {
    char *description = NULL;
    t_ac_rtsp_states rc = AC_STATE_ON_ERROR;

    if(!ac_req_cam_describe(CAM_SESSION, &description)) goto on_exit;
    if(!ac_req_vs_announce(PLAYER_SESSION, description)) goto on_exit;

    rc = AC_STATE_SETUP;
on_exit:
    if(description) free(description);
    return rc;
}
static t_ac_rtsp_states process_setup() {    /* NB! Video stream only!*/

    if(!ac_req_setup(CAM_SESSION, AC_FAKE_CLIENT_PORT)) return AC_STATE_ON_ERROR;
    ag_saveServerPort(CAM_SESSION->video_port);

    if(!ac_req_setup(PLAYER_SESSION, AC_FAKE_CLIENT_PORT)) return AC_STATE_ON_ERROR;
    ag_saveClientPort(PLAYER_SESSION->video_port);

    return AC_STATE_START_PLAY;
}
static t_ac_rtsp_states process_play() {

    if(!ac_req_play(CAM_SESSION)) return AC_STATE_ON_ERROR;
    if(!ac_req_play(PLAYER_SESSION)) return AC_STATE_ON_ERROR;

    if(!start_streaming()) return AC_STATE_ON_ERROR;

    return AC_STATE_PLAYING;
}
static void process_stop() {

    ac_req_teardown(CAM_SESSION);
    ac_req_teardown(PLAYER_SESSION);

    stop_streaming();
}

static void* vc_thread(void* params) {
    t_ac_rtsp_states state;

    pu_log(LL_INFO, "%s started.", AT_THREAD_NAME);

on_reconnect:
    if(!init_proc()) {
        pu_log(LL_ERROR, "%s exiting by hard error", AT_THREAD_NAME);
        say_disconnected_to_vm();
        pthread_exit(NULL);
    }
    state = AC_STATE_CONNECT;

    while(!stop && (state != AC_STATE_ON_ERROR)) {
        switch(state) {
            case AC_STATE_CONNECT:
                pu_log(LL_DEBUG, "%s: State CONNECT processing", AT_THREAD_NAME);
                state = process_connect();
                break;
             case AC_STATE_DESCRIBE:
                pu_log(LL_DEBUG, "%s: State DESCRIBE processing", AT_THREAD_NAME);
                state = process_describe();
                break;
            case AC_STATE_SETUP:
                pu_log(LL_DEBUG, "%s: State SETUP processing", AT_THREAD_NAME);
                state = process_setup(); /* Save VS & CAM UDP ports */
                break;
            case AC_STATE_START_PLAY:
                pu_log(LL_DEBUG, "%s: State START PLAY processing", AT_THREAD_NAME);
                state = process_play();
                break;
            case AC_STATE_PLAYING:
                pu_log(LL_DEBUG, "%s: State PLAYING...", AT_THREAD_NAME);
                sleep(1);
                break;
             default:
                pu_log(LL_ERROR, "%s: Unknown state = %d. Exiting", AT_THREAD_NAME, state);
                state = AC_STATE_ON_ERROR;
                break;
        }
    }
    switch (state) {
        case AC_STATE_ON_ERROR:
            pu_log(LL_ERROR, "%s: Exit by error. Reconnect", AT_THREAD_NAME);
            break;
        case AC_STATE_PLAYING:
            pu_log(LL_INFO, "%s: Exit by stop playing", AT_THREAD_NAME);
            process_stop();
            break;
        default:
            pu_log(LL_ERROR, "%s: Exit by VIDEO_MANAGER request", AT_THREAD_NAME);
            break;
    }
    if((state == AC_STATE_ON_ERROR)) {
        shutdown_proc();
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
        pu_log(LL_WARNING, "%s: Video connector already run. Start ignored", __FUNCTION__);
        return 1;
    }

    pu_log(LL_DEBUG, "%s: VS connection parameters: host = %s, port = %d, vs_session_id = %s", __FUNCTION__, host, port, session_id);
    if(!au_strcpy(IN_PARAMS.host, host, sizeof(IN_PARAMS.host))) return 0;
    if(!au_strcpy(IN_PARAMS.session_id, session_id, sizeof(IN_PARAMS.session_id))) return 0;
    IN_PARAMS.port = AC_PLAYER_VIDEO_PORT;

    struct hostent* hn = gethostbyname(host);
    if(!hn) {
        pu_log(LL_ERROR, "%s: Can't get IP of VS server: %d - %s", __FUNCTION__, h_errno, strerror(h_errno));
        return -1;
    }
    struct sockaddr_in s;
    memcpy(&s.sin_addr, hn->h_addr_list[0], (size_t)hn->h_length);
    char* ip = inet_ntoa(s.sin_addr);
    ag_saveClientIP(ip);
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




