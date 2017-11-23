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
#include "ao_cmd_data.h"
#include "ao_cmd_cloud.h"

#include "ac_rstp.h"

#include "at_video_connector.h"

#include "ag_settings.h"
#include "at_cam_video.h"


#define AT_THREAD_NAME "VIDEO_MANAGER"

/*************************************************************************
 * Local data
 */

#define SM_EXIT 1
#define SM_NOEXTIT 0

static pthread_t id;
static pthread_attr_t attr;

static volatile int stop;       /* Thread stop flag */
static volatile int is_run;

typedef enum {
    AT_UNDEFINED,           /* No connecting parameters */
    AT_INITIAL,             /* Connecting parameters are saved */
    AT_CONNECTING_START,    /* Video connector starts, connection is in process */
    AT_CONNECTED,           /* Connection OK, video connector returns session ID */
    AT_PLAY_START,          /* Streaming starts */
    AT_PLAYING,             /* Video connector reports the streaming is in progress */
    AT_PLAY_STOP,           /* Cloud asks for stop, streaming goes down */
    AT_NO_PLAY              /* Vide connector reportd the streaming is down; goto AT_CONNECTED */
} t_mgr_state;

static t_ao_video_start connection_data;

static t_mgr_state own_status = AT_UNDEFINED;
static pu_queue_t* from_agent;
static pu_queue_t* to_agent;

static t_ao_video_conn_data conn_params;

static void* main_thread(void* params);

static void process_message(const char* msg);
    static int processUndefined(const char* in, char* out, size_t size);
    static int processInitial(const char* in, char* out, size_t size);
    static int processConnectingStart(const char* in, char* out, size_t size);
    static int processConnected(const char* in, char* out, size_t size);
    static int processPlayStart(const char* in, char* out, size_t size);
    static int processPlaying(const char* in, char* out, size_t size);
    static int processPlayStop(const char* in, char* out, size_t size);
    static int processNoPlay(const char* in, char* out, size_t size);
    static void processBad(const char* in, char* out, size_t size, const char* diagnostics, int rc, t_mgr_state new_state);

static int video_start_play();
static int video_stop_play();

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

int at_is_video_mgr_run() {
    return is_run;
}

static void* main_thread(void* params) {

    stop = 0;
    is_run = 1;

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
    at_stop_video_connector();
    is_run = 0;
    pthread_exit(NULL);
}
/* Processing commands from cloud: VIDEO_CONNECT, VIDEO_DISCONNECT, START_PLAY, STOP_PLAY */
static void process_message(const char* msg) {
    char responce[LIB_HTTP_MAX_MSG_SIZE];
    int out = 0;

    while (!out) {
        switch (own_status) {
            case AT_UNDEFINED:
                out = processUndefined(msg, responce, sizeof(responce) - 1);
                break;
            case AT_INITIAL:
                out = processInitial(msg, responce, sizeof(responce) - 1);
                break;
            case AT_CONNECTING_START:
                out = processConnectingStart(msg, responce, sizeof(responce) - 1);
                break;
            case AT_CONNECTED:
                out = processConnected(msg, responce, sizeof(responce) - 1);
                break;
            case AT_PLAY_START:
                out = processPlayStart(msg, responce, sizeof(responce) - 1);
                break;
            case AT_PLAYING:
                out = processPlaying(msg, responce, sizeof(responce) - 1);
                break;
            case AT_PLAY_STOP:
                out = processPlayStop(msg, responce, sizeof(responce) - 1);
                break;
            case AT_NO_PLAY:
                out = processNoPlay(msg, responce, sizeof(responce) - 1);
            default:
                pu_log(LL_ERROR, "%s at unrecognized state. Internal error.", AT_THREAD_NAME);
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
            ag_saveVideoConnectionData(&data);
            own_status++;
/* answer creation should be here! */
            pu_log(LL_DEBUG, "%s: Video server connection info received. %s", AT_THREAD_NAME, in);
            return SM_EXIT;
        case AO_CAM_VIDEO_PLAY_STOP_RESULT:
        case AO_CAM_VIDEO_PLAY_START_RESULT:
        case AO_CAM_VIDEO_CONNECTON_RESULT:
            pu_log(LL_ERROR, "%s Cam is conneting at unuppropriate time %s. Streaming processes will be killed immediately", AT_THREAD_NAME, in);
            at_stop_video_connector();
            return SM_EXIT;
        case AO_COUD_START_VIDEO:
        case AO_CLOUD_STOP_VIDEO:
            pu_log(LL_ERROR, "%s: Video server connection parameters were not sent! %s", AT_THREAD_NAME, in);
            return SM_EXIT;
        default:
            pu_log(LL_ERROR, "%s: Unrecognozed command %d received %s", AT_THREAD_NAME, msg_type, in);
            return SM_EXIT;
    }
}
static int processInitial(const char* in, char* out, size_t size) {
    t_ao_msg_type msg_type;
    t_ao_msg data;
    switch(msg_type=ao_cloud_decode(in, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            ag_dropVideoConnectionData();
            own_status = AT_UNDEFINED;
            pu_log(LL_WARNING, "%s Video server connection parameters were dropped. %s", AT_THREAD_NAME, in);
            return SM_NOEXTIT;
        case AO_COUD_START_VIDEO:
            at_start_video_connector();
            own_status = AT_CONNECTING_START;
            return SM_EXIT;
        case AO_CLOUD_STOP_VIDEO:
            pu_log(LL_WARNING, "%s Cloud requested stop play for non-connected Cam", AT_THREAD_NAME);
            return SM_EXIT;
        case AO_CAM_VIDEO_CONNECTON_RESULT:
        case AO_CAM_VIDEO_PLAY_START_RESULT:
        case AO_CAM_VIDEO_PLAY_STOP_RESULT:
            pu_log(LL_ERROR, "%s %s - Cam is not connected yet! Streaming/connection wil be killed", AT_THREAD_NAME, in);
        default:
            pu_log(LL_ERROR, "%s: Unrecognozed command %d received %s", AT_THREAD_NAME, msg_type, in);
            return SM_EXIT;
    }
}
static int processConnectingStart(const char* in, char* out, size_t size) {
    t_ao_msg_type msg_type;
    t_ao_msg data;
    switch(msg_type=ao_cloud_decode(in, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            at_stop_video_connector();
            ag_dropVideoConnectionData();
            own_status = AT_UNDEFINED;
            pu_log(LL_WARNING, "%s Video server connection parameters were dropped. %s", AT_THREAD_NAME, in);
            return SM_NOEXTIT;
        case AO_COUD_START_VIDEO:
            pu_log(LL_WARNING, "%s %s - Camera is already on connecting state", AT_THREAD_NAME, in);
            return SM_EXIT;
        case AO_CLOUD_STOP_VIDEO:
            pu_log(LL_WARNING, "%s %s - Cloud requested stop play. Cam in connecting state. Ignored", AT_THREAD_NAME,
                   in);
            return SM_EXIT;
        case AO_CAM_VIDEO_CONNECTON_RESULT:
/* TODO - prepare and send answer to the cloud */
            own_status = AT_CONNECTED;
            return SM_EXIT;
        case AO_CAM_VIDEO_PLAY_START_RESULT:
            pu_log(LL_WARNING, "%s %s - Cam already start play!", AT_THREAD_NAME, in);
/* TODO - prepare and send answer to the cloud */
            own_status = AT_PLAYING;
            return SM_EXIT;
        case AO_CAM_VIDEO_PLAY_STOP_RESULT:
            pu_log(LL_WARNING,
                   "%s %s Cam is not playing. At least, nobody here didn't know about it. Set to 'CONNECTED'",
                   AT_THREAD_NAME, in);
            own_status = AT_CONNECTED;
            return SM_EXIT;
        default:
            pu_log(LL_ERROR, "%s: Unrecognozed command %d received %s", AT_THREAD_NAME, msg_type, in);
            return SM_EXIT;
    }
}
static int processConnected(const char* in, char* out, size_t size) {
    t_ao_msg_type msg_type;
    t_ao_msg data;
    switch(msg_type=ao_cloud_decode(in, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            at_stop_video_connector();
            own_status = AT_INITIAL;
            return SM_NOEXTIT;
        case AO_COUD_START_VIDEO:
            pu_log(LL_WARNING, "%s %s - Start play command came during the Cam just connected. Too quick.", AT_THREAD_NAME, in);
            return SM_EXIT;
        case AO_CLOUD_STOP_VIDEO:
            pu_log(LL_WARNING, "%s %s - Stop play command came during the Cam is just connected. Too quick", AT_THREAD_NAME, in);
            return SM_EXIT;
        case AO_CAM_VIDEO_CONNECTON_RESULT:
            pu_log(LL_WARNING, "%s %s - Cam is already connected", AT_THREAD_NAME, in);
/* TODO - prepare and send answer to the cloud */
            return SM_EXIT;
        case AO_CAM_VIDEO_PLAY_START_RESULT:
            pu_log(LL_WARNING, "%s %s - Cam is playing and here we thought it was just connected...", AT_THREAD_NAME, in);
            own_status = AT_PLAYING;
/* TODO - prepare and send answer to the cloud */
            return SM_EXIT;
        case AO_CAM_VIDEO_PLAY_STOP_RESULT:
            pu_log(LL_WARNING, "%s %s - Cam reported stop playing and we here just in 'Connected' state", AT_THREAD_NAME, in);
/* TODO - prepare and send answer to the cloud */
            return SM_EXIT;
        default:
            pu_log(LL_ERROR, "%s: Unrecognozed command %d received %s", AT_THREAD_NAME, msg_type, in);
            return SM_EXIT;
    }
}
static int processPlayStart(const char* in, char* out, size_t size) {
    t_ao_msg_type msg_type;
    t_ao_msg data;
    switch(msg_type=ao_cloud_decode(in, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            at_stop_video_connector();
            own_status = AT_INITIAL;
            return SM_NOEXTIT;
        case AO_COUD_START_VIDEO:
            pu_log(LL_WARNING, "%s %s - Start play command came during the Cam already trying to start play.", AT_THREAD_NAME, in);
            return SM_EXIT;
        case AO_CLOUD_STOP_VIDEO:
            pu_log(LL_WARNING, "%s %s - Stop play command came during the Cam is trying to start play. Too quick", AT_THREAD_NAME, in);
            return SM_EXIT;
        case AO_CAM_VIDEO_CONNECTON_RESULT:
            pu_log(LL_WARNING, "%s %s - Cam is already connected and start playing now", AT_THREAD_NAME, in);
/* TODO - prepare and send answer to the cloud */
            return SM_EXIT;
        case AO_CAM_VIDEO_PLAY_START_RESULT:
            own_status = AT_PLAYING;
/* TODO - prepare and send answer to the cloud */
            return SM_EXIT;
        case AO_CAM_VIDEO_PLAY_STOP_RESULT:
            pu_log(LL_WARNING, "%s %s - Cam reported stop playing and we here just trying to start play!", AT_THREAD_NAME, in);
            own_status = AT_CONNECTED;
/* TODO - prepare and send answer to the cloud */
            return SM_EXIT;
        default:
            pu_log(LL_ERROR, "%s: Unrecognozed command %d received %s", AT_THREAD_NAME, msg_type, in);
            return SM_EXIT;
    }
}
static int processPlaying(const char* in, char* out, size_t size) {
    t_ao_msg_type msg_type;
    t_ao_msg data;
    switch(msg_type=ao_cloud_decode(in, &data)) {
        case AO_CLOUD_VIDEO_PARAMS:
            at_stop_video_connector();
            own_status = AT_INITIAL;
            return SM_NOEXTIT;
        case AO_COUD_START_VIDEO:
            pu_log(LL_WARNING, "%s %s - Cloud requested play for already playimg Cam", AT_THREAD_NAME, in);
            return SM_EXIT;
        case AO_CLOUD_STOP_VIDEO:
            pu_log(LL_WARNING, "%s %s - Cloud requested stop play for playimg Cam. Wait.", AT_THREAD_NAME, in);
            return SM_EXIT;
        case AO_CAM_VIDEO_CONNECTON_RESULT:
            pu_log(LL_WARNING, "%s %s - Get connection result... Cam seems already connected and playing now", AT_THREAD_NAME, in);
/* TODO - prepare and send answer to the cloud */
            return SM_EXIT;


        default:
            pu_log(LL_ERROR, "%s: Unrecognozed command %d received %s", AT_THREAD_NAME, msg_type, in);
            processBad(in, out, size, "Vide Manager received undefined command.", -3, own_status);
            return SM_EXIT;

    }
}
static int processPlayStop(const char* in, char* out, size_t size) {

}
static int processNoPlay(const char* in, char* out, size_t size) {

}
static void processBad(const char* in, char* out, size_t size, const char* diagnostics, int rc, t_mgr_state new_state) {
    t_ao_msg err;
    err.own_error.msg_type = AO_OWN_ERROR;
    err.own_error.rc = rc;
    strncpy(err.own_error.error, diagnostics, sizeof(err.own_error.error)-1);
    ao_cloud_encode(err, out, size);

    own_status = new_state;
}

static int video_start_play() {
    return 0;
}
static int video_stop_play() {

}
