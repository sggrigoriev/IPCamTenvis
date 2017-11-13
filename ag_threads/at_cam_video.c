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
#include <ag_converter/ao_cmd_data.h>

#include "pu_logger.h"
#include "pu_queue.h"

#include "aq_queues.h"
#include "ao_cmd_cloud.h"

#include "ac_rstp.h"

#include "at_cam_video_read.h"
#include "at_cam_video_write.h"

#include "at_cam_video.h"

#define AT_THREAD_NAME "VIDEO_MANAGER"

/*************************************************************************
 * Local data
 */

static pthread_t id;
static pthread_attr_t attr;

static volatile int stop;       /* Thread stop flag */

typedef enum {AT_UNDEFINED_STATE, AT_INITIAL_STATE, AT_CONNECTED_STATE, AT_PLAYING_STATE} t_mgr_state;

static t_ao_video_start connection_data;

static t_mgr_state own_status = AT_INITIAL_STATE;
static pu_queue_t* from_agent;
static pu_queue_t* to_agent;

static t_ao_video_conn_data conn_params;

static void* main_thread(void* params);

static void process_message(const char* msg);
    static int processUndefined(const char* in, char* out, size_t size);
    static int processInitial(const char* in, char* out, size_t size);
    static int processConnected(const char* in, char* out, size_t size);
    static int processPlaying(const char* in, char* out, size_t size);
    static void processBad(const char* in, char* out, size_t size, const char* diagnostics, int rc, t_mgr_state new_state);

static void stop_video_threads();
static void start_video_threads();

int at_start_video_mgr() {

    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &main_thread, NULL)) return 0;
    return 1;
}

int at_stop_video_mgr() {
    void* ret;

    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    return 1;
}

void at_set_stop_video_mgr() {
    stop = 1;
}

/*******************************************************************
 * Local functions implementation
 */



static void* main_thread(void* params) {

    stop = 0;

    unsigned int events_timeout = 60; /* Wait 60 seconds */
    pu_queue_event_t events;

    pu_queue_msg_t msg[LIB_HTTP_MAX_MSG_SIZE] = {0};    /* The only main thread's buffer! */

    from_agent = aq_get_gueue(AQ_ToVideoMgr);
    to_agent = aq_get_gueue(AQ_FromVideoMgr);

    events = pu_add_queue_event(pu_create_event_set(), AQ_ToVideoMgr);

    while(!stop) {
        size_t len = sizeof(msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;

        switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case AQ_ToVideoMgr:
                while(pu_queue_pop(from_agent, msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Agent main %s", AT_THREAD_NAME, msg);
                    process_message(msg);
                    len = sizeof(msg);
                }
                break;
            case AQ_Timeout:
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
/* shutdown procedure */
    stop_video_threads();
    pthread_exit(NULL);
}
/* Processing commands from cloud: VIDEO_CONNECT, VIDEO_DISCONNECT, START_PLAY, STOP_PLAY */
static void process_message(const char* msg) {
    char responce[LIB_HTTP_MAX_MSG_SIZE];
    int out = 0;

    while (!out) {
        switch (own_status) {
            case AT_UNDEFINED_STATE:
                out = processUndefined(msg, responce, sizeof(responce) - 1);
                break;
            case AT_INITIAL_STATE:
                out = processInitial(msg, responce, sizeof(responce) - 1);
                break;
            case AT_CONNECTED_STATE:
                out = processConnected(msg, responce, sizeof(responce) - 1);
                break;
            case AT_PLAYING_STATE:
                out = processPlaying(msg, responce, sizeof(responce) - 1);
                break;
            default:
                processBad(msg, responce, sizeof(responce)-1, "Video Manager in unrecognized state. Loot at Agent's log!", -1, AT_UNDEFINED_STATE);
                out = 1;
                break;
        }
    }
    pu_queue_push(to_agent, responce, strlen(responce) + 1);
}

static int processUndefined(const char* in, char* out, size_t size) {
    t_ao_msg_type msg_type;
    t_ao_msg data;
    switch(msg_type=ao_cloud_decode(in, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            conn_params = data.video_conn_data;
            own_status = AT_INITIAL_STATE;
            pu_log(LL_DEBUG, "%s: Video server connection info received. %s", AT_THREAD_NAME, in);
            return 1;
        case AO_COUD_START_VIDEO:
        case AO_CLOUD_STOP_VIDEO:
            pu_log(LL_ERROR, "%s: Video server connection parameters were not sent! %s", AT_THREAD_NAME, in);
            processBad(in, out, size, "Can not perform: connection parameers were not sent", -2, AT_UNDEFINED_STATE);
            return 1;
        default:
            pu_log(LL_ERROR, "%s: Unrecognozed command %d received %s", AT_THREAD_NAME, msg_type, in);
            processBad(in, out, size, "Vide Manager received undefined command.", -3, own_status);
            return 1;
    }
}
static int processInitial(const char* in, char* out, size_t size) {
    t_ao_msg_type msg_type;
    t_ao_msg data;
    switch(msg_type=ao_cloud_decode(in, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            conn_params = data.video_conn_data;
            pu_log(LL_WARNING, "%s Vide server parameters updated. %s", AT_THREAD_NAME, in);
            return 1;
        case AO_COUD_START_VIDEO:
            if(!ac_connect(conn_params, out, size))
            break;
        case AO_CLOUD_STOP_VIDEO:
            break;
        default:
            break;
    }
}
static int processConnected(const char* in, char* out, size_t size) {
    t_ao_msg_type msg_type;
    t_ao_msg data;
    switch(msg_type=ao_cloud_decode(in, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            break;
        case AO_COUD_START_VIDEO:
            break;
        case AO_CLOUD_STOP_VIDEO:
            break;
        default:
            break;
    }
}
static int processPlaying(const char* in, char* out, size_t size) {
    t_ao_msg_type msg_type;
    t_ao_msg data;
    switch(msg_type=ao_cloud_decode(in, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            break;
        case AO_COUD_START_VIDEO:
            break;
        case AO_CLOUD_STOP_VIDEO:
            break;
        default:
            break;
    }
}
static void processBad(const char* in, char* out, size_t size, const char* diagnostics, int rc, t_mgr_state new_state) {
    t_ao_msg err;
    err.own_error.msg_type = AO_OWN_ERROR;
    err.own_error.rc = rc;
    strncpy(err.own_error.error, diagnostics, sizeof(err.own_error.error)-1);
    ao_cloud_encode(err, out, size);

    own_status = new_state;
}


    switch(msg_type=ao_cloud_decode(msg, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            switch(own_status) {
                case AT_UNDEFINED_STATE:
                     break;
                case AT_INITIAL_STATE:
                    conn_params = data.video_start;
                    pu_log(LL_WARNING, "%s: connection parameters updated: %s", AT_THREAD_NAME, msg);
                    break;
                case AT_CONNECTED_STATE:
                    ac_disconnect(responce, sizeof(responce)-1);
                    pu_log(LL_WARNING, "%s disconnected %s", AT_THREAD_NAME, msg);
                     break;
                case AT_PLAYING_STATE:
                    ac_stop_play(responce, sizeof(responce)-1);
                    pu_log(LL_WARNING, "%s stop video play %s", AT_THREAD_NAME, msg);
                    ac_disconnect(responce, sizeof(responce)-1);
                    pu_log(LL_WARNING, "%s disconnected %s", AT_THREAD_NAME, msg);
                    break;
                default:
                    pu_log(LL_ERROR, "%s, unrecognized own_status %d, operation %s ignored", AT_THREAD_NAME, own_status, msg);
                    break;
            }
            conn_params = data.video_start;
            own_status = AT_INITIAL_STATE;
            break;
        case AO_COUD_START_VIDEO:
            switch(own_status) {
                 case AT_UNDEFINED_STATE:
                    pu_log(LL_ERROR, "%s connection parameters were not sent %s", AT_THREAD_NAME, msg);
                    break;
                case AT_INITIAL_STATE:
                    if(!ac_connect(conn_params, responce, sizeof(responce)-1)) {
                        pu_log(LL_ERROR, "%s, connection error %s", responce);
                    }
                    else {
                        own_status = AT_CONNECTED_STATE;
                    }
                    break;
                case AT_CONNECTED_STATE:    /* Start video command came second time - let's play! */
                    if(!ac_start_play(conn_params, responce, sizeof(responce)-1)) {
                        pu_log(LL_ERROR, "%s: can't start play video: %s", AT_THREAD_NAME, responce);
                    }
                    if(!ac_connect(conn_params, responce, sizeof(responce)-1)) {
                        pu_log(LL_ERROR, "%s, connection error %s", responce);
                    }
                    else {
                        own_status = AT_CONNECTED_STATE;
                    }
                    break;

            }
            break;
        case AO_CLOUD_STOP_VIDEO:
            break;
        default:
            break;
    }

}
static void stop_video_threads() {
    at_set_stop_video_read();
    at_set_stop_video_write();

    at_stop_video_read();
    at_set_stop_video_write();
}
static void start_video_threads() {
    at_start_video_write(conn_params);
    at_start_video_read(conn_params);
}
