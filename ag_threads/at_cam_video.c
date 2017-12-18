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
 Created by gsg on 17/10/17.
*/

#include <pthread.h>
#include <memory.h>

#include "pu_logger.h"
#include "pu_queue.h"

#include "aq_queues.h"
#include "ao_cmd_data.h"
#include "ag_settings.h"
#include "ao_cmd_proxy.h"
#include "ac_cloud.h"
#include "at_video_connector.h"
#include "at_ws.h"

#include "at_cam_video.h"

#define AT_THREAD_NAME "VIDEO_MANAGER"

/*************************************************************************
 * Local data
 */

static pthread_t id;
static pthread_attr_t attr;

static volatile int stop = 1;       /* Thread stop flag */

typedef enum {
    AT_ERROR,
    AT_GOT_PROXY_INFO,              /* Got proxy/auth info - could start params */
    AT_GOT_VIDEO_CONN_INFO,         /* Got parameters! */
    AT_READY,
    AT_PLAY
} t_mgr_state;

static t_mgr_state own_status = AT_GOT_PROXY_INFO;

static pu_queue_t* from_ws;

static t_ao_conn video_conn = {0};
static t_ao_conn ws_conn = {0};

/*******************************************************************
* Local functions implementation
*/
static t_mgr_state start_ws_thread() {
    if(!start_ws(ws_conn.url, ws_conn.port, "/streaming/camera", ws_conn.auth)) {
        pu_log(LL_ERROR, "%s - %s: Error start WEB socket connector, exit.", AT_THREAD_NAME, __FUNCTION__);
        return AT_ERROR;
    }
    return AT_READY;
}
static t_mgr_state start_vc_therad() {
    if(own_status != AT_READY) {
        pu_log(LL_ERROR, "%s - %s: Wrong entry status = %d instead of %d, exit.", AT_THREAD_NAME, __FUNCTION__, own_status, AT_READY);
        return AT_ERROR;
    }
    if(!at_start_video_connector(video_conn.url, video_conn.port, video_conn.auth)) {
        pu_log(LL_ERROR, "%s - %s: Error start video connector, exit.", AT_THREAD_NAME, __FUNCTION__);
        return AT_ERROR;
    }
    return AT_PLAY;
}
/* Get video params params from cloud: 3 steps from https://presence.atlassian.net/wiki/spaces/EM/pages/164823041/Setup+IP+Camera+connection */
static t_mgr_state get_vs_conn_params(t_ao_conn* video, t_ao_conn* ws) {
    if(!ac_cloud_get_params(video->url, sizeof(video->url), &video->port, video->auth, sizeof(video->auth), ws->url, sizeof(ws->url), &ws->port, ws->auth, sizeof(ws->auth))) {
        return AT_ERROR;
    }
    pu_log(LL_DEBUG, "%s: Video Connection parameters: URL = %s, PORT = %d, SessionId = %s", __FUNCTION__, video->url, video->port, video->auth);
    pu_log(LL_DEBUG, "%s: WS Connection parameters: URL = %s, PORT = %d, SessionId = %s", __FUNCTION__, ws->url, ws->port, ws->auth);
    return AT_GOT_VIDEO_CONN_INFO;
}

static t_mgr_state process_ws_message(const char* msg) {
    if(own_status != AT_READY) {
        pu_log(LL_ERROR, "%s - %s: Bad status = %d! Sould be %d", AT_THREAD_NAME, __FUNCTION__, own_status, AT_READY);
        return AT_ERROR;
    }
    if(!strcmp(msg, DEFAULT_WC_START_PLAY)) return start_vc_therad();

    if(!strcmp(msg, DEFAULT_WC_STOP_PLAY)) {
        at_stop_video_connector();
        pu_log(LL_INFO, "%s: Video connector stop", AT_THREAD_NAME);
        return AT_READY;
    }
    return own_status;
}

static void* main_thread(void* params) {

    pu_queue_msg_t msg[LIB_HTTP_MAX_MSG_SIZE] = {0};    /* The only main thread's buffer! */

    pu_queue_event_t events;
    events = pu_add_queue_event(pu_create_event_set(), AQ_FromWS);
    from_ws = aq_get_gueue(AQ_FromWS);

    own_status = AT_GOT_PROXY_INFO;

    while(!stop) {
        size_t len = sizeof(msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;
        if((own_status == AT_PLAY) || (own_status == AT_READY)) {
            switch (ev = pu_wait_for_queues(events, 1)) {
                case AQ_FromWS:
                    while (pu_queue_pop(from_ws, msg, &len)) {
                        pu_log(LL_DEBUG, "%s: got message from the Web socket thread %s", AT_THREAD_NAME, msg);
                        own_status = process_ws_message(msg);
                        len = sizeof(msg);
                    }
                    break;
                case AQ_Timeout:
//                pu_log(LL_DEBUG, "%s: timeout", AT_THREAD_NAME);
                    break;
                case AQ_STOP:
                    stop = 1;
                    pu_log(LL_INFO, "%s received STOP event. Terminated", AT_THREAD_NAME);
                    break;
                default:
                    pu_log(LL_ERROR, "%s: Undefined event %d on wait.", AT_THREAD_NAME, ev);
                    break;
            }
        }
        switch(own_status) {        /* State machine */
            case AT_GOT_PROXY_INFO:
                own_status = get_vs_conn_params(&video_conn, &ws_conn);
                break;
            case AT_GOT_VIDEO_CONN_INFO:
                own_status = start_ws_thread(); /* -> READY */
                break;
            case AT_READY:
                if(!is_ws_run()) {
                    pu_log(LL_WARNING, "%s: Wideo socket tread restart", AT_THREAD_NAME);
                    own_status = start_ws_thread();
                }
                break;
            case AT_PLAY:
                if(!is_ws_run()) {
                    pu_log(LL_WARNING, "%s: Wideo socket tread restart", AT_THREAD_NAME);
                    if(is_video_connector_run()) at_stop_video_connector();
                    own_status = start_ws_thread();
                }
                else if(!is_video_connector_run()) {
                    pu_log(LL_WARNING, "%s: Wideo connector tread restart", AT_THREAD_NAME);
                    own_status = AT_READY;
                    own_status = start_vc_therad();
                }
                break;
            case AT_ERROR:
                pu_log(LL_DEBUG, "%s: Error. Stop the thread", AT_THREAD_NAME);
                stop = 1;
                break;
            default:
                break;
        }
    }
/* shutdown procedure */
    if(is_video_connector_run()) at_stop_video_connector();
    if(is_ws_run()) stop_ws();
    pthread_exit(NULL);
}

/**********************************************************************
 * Public functions
 */

int at_start_video_mgr() {
    if(is_video_mgr_run() ) {
        pu_log(LL_WARNING, "%s - %s The thread is already runninng", AT_THREAD_NAME, __FUNCTION__);
        return 1;
    }
    stop = 0;
    if(pthread_attr_init(&attr)) { stop = 1;return 0;}
    if(pthread_create(&id, &attr, &main_thread, NULL)) { stop = 1; return 0;}

    pu_log(LL_INFO, "%s started", AT_THREAD_NAME);
    return 1;
}
int at_stop_video_mgr() {
    void* ret;
    if(!is_video_mgr_run() ) {
        pu_log(LL_WARNING, "%s - %s: The thread is already stop. Stop ignored", AT_THREAD_NAME, __FUNCTION__);
        return 1;
    }
    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    return 1;
}
void at_set_stop_video_mgr() {
    stop = 1;
}
int is_video_mgr_run() {
    return !stop;
}
