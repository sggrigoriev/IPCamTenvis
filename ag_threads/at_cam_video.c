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
#include "lib_timer.h"

#include "aq_queues.h"
#include "ag_settings.h"
#include "ao_cmd_data.h"
#include "ao_cmd_cloud.h"
#include "at_video_connector.h"

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

typedef enum {
    AT_INITIAL,                 /* No connection info */
    AT_OWN_INFO,                /* Got connectin info */
    AT_SESSION_ID_AWAITING,     /* Wait for sesion detais (session_id) */
    AT_CONNECTING,              /* Wait for VC report about connection to Wowza */
    AT_CONN_RESP_AWAITING,      /* Wait answer from cloud for start stream = 1 */
    AT_CONNECTED,               /* VC works and does its own business */
/*------------------------------------------------------------------------------------------*/
    AT_DISCONNECT_RESP_AWAITING /* VC disconnected, send start session = 0 to cloud, wait for respond */
} t_mgr_state;

static t_mgr_state own_status = AT_INITIAL;

static pu_queue_t* from_agent;
static pu_queue_t* to_agent;

/*-------------------- Data for repeating messages to the cloud in case "No answer" -----*/
static lib_timer_clock_t session_id_to;
static int session_id_to_up = 0;
static lib_timer_clock_t conn_resp_to;
static int conn_resp_to_up = 0;
static lib_timer_clock_t disconn_resp_to;
static int disconn_resp_to_up = 0;
static char buffered_responce[LIB_HTTP_MAX_MSG_SIZE];
/*******************************************************
 * Resends buffered request if some timeout is exceeded
 */
static void resend_request_to_cloud();
/**************************************************************************
 * Make thr message with session detaild request to clouf
 * @param msg   - buffer
 * @param size  - buffer size
 * @return - pointer to the buffer with message
 */
static const char* prepare_sesson_details_request(char* msg, size_t size);
static const char* prepare_unsucc_cam_connection(char* msg, size_t size);   /* stream statis = 0 instead of 1 due to error */
static const char* prepare_cam_connected(char* msg, size_t size);           /* stream statis = 1 send to cloud */
static const char* prepare_cam_disconnection(char* msg, size_t size);       /* stream satus = 0 send to cloud */
static const char* prepare_bad_cam_disconnection(char* msg, size_t size);   /* Disconnected because of error */
/*---------------------------------------------------------------------------------------*/

static void* main_thread(void* params);

static void process_message(const char* msg);
    static int processInitial(const char* in, char* out, size_t size);
    static int processOwnInfo(const char* in, char* out, size_t size);
    static int processSessionIdAwaiting(const char* in, char* out, size_t size);
    static int processConnecting(const char* in, char* out, size_t size);
    static int processConnRespAwaiting(const char* in, char* out, size_t size);
    static int processConnected(const char* in, char* out, size_t size);
    static int processDsconnectRespAwaiting(const char* in, char* out, size_t size);

/**********************************************************************
 * Public functions
 */

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

    unsigned int events_timeout = 1; /* Wait 1 second - timeouts for respond from cloud should be enabled! */
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
                resend_request_to_cloud();
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
    pthread_exit(NULL);
}

/* VM's state maschine */
static void process_message(const char* msg) {
    char responce[LIB_HTTP_MAX_MSG_SIZE];
    int out = 0;

    while (!out) {
        switch (own_status) {
            case AT_INITIAL:
                out = processInitial(msg, responce, sizeof(responce) - 1);
                break;
            case AT_OWN_INFO:
                out = processOwnInfo(msg, responce, sizeof(responce) - 1);
                break;
            case AT_SESSION_ID_AWAITING:
                out = processSessionIdAwaiting(msg, responce, sizeof(responce) - 1);
                break;
            case AT_CONNECTING:
                out = processConnecting(msg, responce, sizeof(responce) - 1);
                break;
            case AT_CONN_RESP_AWAITING:
                out = processConnRespAwaiting(msg, responce, sizeof(responce) - 1);
                break;
            case AT_CONNECTED:
                out = processConnected(msg, responce, sizeof(responce) - 1);
                break;
            case AT_DISCONNECT_RESP_AWAITING:
                out = processDsconnectRespAwaiting(msg, responce, sizeof(responce) - 1);
                break;
            default:
                pu_log(LL_ERROR, "%s at unrecognized state. Internal error.", AT_THREAD_NAME);
                out = 1;
                break;
        }
        if(strlen(responce)) pu_queue_push(to_agent, responce, strlen(responce) + 1);
    }
}

static int processInitial(const char* in, char* out, size_t size) { /* No connection info */
    t_ao_msg data;
    out[0] = '\0';

    switch (ao_cloud_decode(in, &data)) {
        case AO_IN_VIDEO_PARAMS:
            ag_saveVideoConnectionData(data.in_video_params);
            pu_log(LL_DEBUG, "%s %s - Connection parameters saved", AT_THREAD_NAME, in);
            own_status = AT_OWN_INFO;
            break;
        default:
            pu_log(LL_ERROR, "%s %s - Can't process the message in Initial state. Connection parameters needed", AT_THREAD_NAME, in);
            break;
    }
    return SM_EXIT;
}
static int processOwnInfo(const char* in, char* out, size_t size) { /* Connection info exists */
    t_ao_msg data;
    out[0] = '\0';

    switch (ao_cloud_decode(in, &data)) {
        case AO_IN_VIDEO_PARAMS:            /* Reconnect case */
            ag_dropVideoConnectionData();
            own_status = AT_INITIAL;
            pu_log(LL_INFO, "%s %s - New connection data received on AT_OWN_INFO state.", AT_THREAD_NAME, in);
            return SM_NOEXTIT;
        case AO_IN_START_STREAM_0:  /* Connect request from cloud */
            prepare_sesson_details_request(out, size);
            lib_timer_init(&session_id_to, /*ag_getSessionIdTO*/ 1);
            session_id_to_up = 1;
            own_status = AT_SESSION_ID_AWAITING;
            pu_log(LL_INFO, "%s %s - Video connect requested. Waiting for stream details", AT_THREAD_NAME, in);
            break;
        default:
            pu_log(LL_ERROR, "%s %s - Can't process the message in OwnInfo state. Connection to video request expected", AT_THREAD_NAME, in);
            break;
    }
    return SM_EXIT;
}
static int processSessionIdAwaiting(const char* in, char* out, size_t size) {
    t_ao_msg data;
    out[0] = '\0';

    switch (ao_cloud_decode(in, &data)) {
        case AO_IN_VIDEO_PARAMS:    /* Reconnect case */
            ag_dropVideoConnectionData();
            session_id_to_up = 0;
            own_status = AT_INITIAL;
            pu_log(LL_INFO, "%s %s - New connection data received on StartStreamAwaiting state.", AT_THREAD_NAME, in);
            return SM_NOEXTIT;
        case AO_IN_STREAM_SESS_DETAILS:  /* Cloud provides stream session details */
            session_id_to_up = 0;   /* Dpop timeout - got the reapond */
            ag_saveStreamDetails(data.in_stream_sess_details);
        case AO_CAM_DISCONNECTED: {  /* Reconnection case */
            char host[5000] = {0};
            int port;
            char session[1000] = {0};
            at_start_video_connector(host, port, session);     /* start VC to connect */
            own_status = AT_CONNECTING;
            pu_log(LL_INFO, "%s %s - Got session details, start connection process", AT_THREAD_NAME, in);
        }
            break;
        default:
            pu_log(LL_ERROR, "%s %s - Can't process the message in SessionIdAwaiting state. Session details expected", AT_THREAD_NAME, in);
            break;
    }
    return SM_EXIT;
}
static int processConnecting(const char* in, char* out, size_t size) {
    t_ao_msg data;
    out[0] = '\0';

    switch (ao_cloud_decode(in, &data)) {
        case AO_IN_VIDEO_PARAMS:    /* Reconnect case */
            at_stop_video_connector();
            ag_dropVideoConnectionData();
            own_status = AT_INITIAL;
            pu_log(LL_INFO, "%s %s - New connection data received on Connecting state. Reconnect", AT_THREAD_NAME, in);
            return SM_NOEXTIT;
        case AO_CAM_CONNECTED:  /* Cam reported the connection state !! from camera!*/
            if(data.cam_connected.connected) { /* VC succesfully connected to Video server */
                prepare_cam_connected(out, size);
                lib_timer_init(&conn_resp_to, ag_getConnectRespTO());
                conn_resp_to_up = 1;
                own_status = AT_CONN_RESP_AWAITING;
                pu_log(LL_INFO, "%s %s - Vide server connected, Wait the responce from the cloud", AT_THREAD_NAME, in);
            }
            else {  /* Alas! VC could not connect to the video server */
                prepare_unsucc_cam_connection(out, size);
                at_stop_video_connector();
                own_status = AT_SESSION_ID_AWAITING;
                pu_log(LL_ERROR, "%s %s - Cam could not connect to the video server. Reconnect Cam", AT_THREAD_NAME, in);
                return SM_NOEXTIT;
            }
            break;
        default:
            pu_log(LL_ERROR, "%s %s - Can't process the message in Connecting state. Session details expected", AT_THREAD_NAME, in);
            break;
    }
    return SM_EXIT;
}
static int processConnRespAwaiting(const char* in, char* out, size_t size) {
    t_ao_msg data;
    out[0] = '\0';

    switch (ao_cloud_decode(in, &data)) {
        case AO_IN_VIDEO_PARAMS:    /* Reconnect case */
            conn_resp_to_up = 0;
            at_stop_video_connector();
            ag_dropVideoConnectionData();
            own_status = AT_INITIAL;
            pu_log(LL_INFO, "%s %s - New connection data received on Connected state. Stop video and reconnect", AT_THREAD_NAME, in);
            return SM_NOEXTIT;
        case AO_CAM_DISCONNECTED:      /* Due to some internal reason cam disconnected. Bad... */
            at_stop_video_connector();
            own_status = AT_SESSION_ID_AWAITING; /*  Goto reconnect */
            pu_log(LL_ERROR, "%s %s - Cam could not connect to the video server. Trying to reconnect Cam", AT_THREAD_NAME, in);
            return SM_NOEXTIT;
        case AO_IN_SS_TO_1_RESP:  /* Cloud reported the connection state !! From cloud !!*/
            conn_resp_to_up = 0;
            own_status = AT_CONNECTED;
            pu_log(LL_INFO, "%s %s - Cloud responds on video sever connection", AT_THREAD_NAME, in);
            break;
        default:
            pu_log(LL_ERROR, "%s %s - Can't process the message in ConnRespAwaiting state. Session details expected", AT_THREAD_NAME, in);
            break;
    }
    return SM_EXIT;
}
static int processConnected(const char* in, char* out, size_t size) {
    t_ao_msg data;
    out[0] = '\0';

    switch (ao_cloud_decode(in, &data)) {
        case AO_IN_VIDEO_PARAMS:    /* Reconnect case */
            at_stop_video_connector();
            ag_dropVideoConnectionData();
            own_status = AT_INITIAL;
            pu_log(LL_INFO, "%s %s - New connection data received on Connected state. Stop video and reconnect", AT_THREAD_NAME, in);
            return SM_NOEXTIT;
        case AO_CAM_DISCONNECTED:      /* Just disconnected. Maybe this is correct, maybe not */
            if(data.cam_disconnected.error_disconnection) { /* Bad reason - some error */
                prepare_bad_cam_disconnection(out, size);
                at_stop_video_connector();
                own_status = AT_SESSION_ID_AWAITING; /*  Goto reconnect */
                pu_log(LL_ERROR, "%s %s - Cam disconnectd from the video server by error. Trying to reconnect Cam", AT_THREAD_NAME, in);
            }
            else {  /* Disconnected by user */
                at_stop_video_connector();
                prepare_cam_disconnection(out, size);
                disconn_resp_to_up = 1;
                lib_timer_init(&disconn_resp_to, ag_getDisconnectRespTO());
                own_status = AT_DISCONNECT_RESP_AWAITING;
            }
            break;
        default:
            pu_log(LL_ERROR, "%s %s - Can't process the message in Connected state. Session details expected", AT_THREAD_NAME, in);
            break;
    }
    return SM_EXIT;
}
static int processDsconnectRespAwaiting(const char* in, char* out, size_t size) {
    t_ao_msg data;
    out[0] = '\0';

    switch (ao_cloud_decode(in, &data)) {
        case AO_IN_VIDEO_PARAMS:    /* Reconnect case */
            disconn_resp_to_up = 0;
            ag_dropVideoConnectionData();
            own_status = AT_INITIAL;
            pu_log(LL_INFO, "%s %s - New connection data received on Connected state. Stop video and reconnect", AT_THREAD_NAME, in);
            return SM_NOEXTIT;
        case AO_IN_SS_TO_0_RESP:      /* Due to some internal reason cam disconnected. Bad... */
            disconn_resp_to_up = 0;
            own_status = AT_OWN_INFO;
            break;
        default:
            pu_log(LL_ERROR, "%s %s - Can't process the message in DsconnectRespAwaiting state. Session details expected", AT_THREAD_NAME, in);
            break;
    }
    return SM_EXIT;
}

static void resend_request_to_cloud() {
    int resend = 0;

    if (session_id_to_up && lib_timer_alarm(session_id_to)) {
        resend = 1;
        lib_timer_init(&session_id_to, ag_getSessionIdTO());
    }
    else if (conn_resp_to_up && lib_timer_alarm(conn_resp_to)) {
        resend = 1;
        lib_timer_init(&conn_resp_to, ag_getConnectRespTO());
    }
    else if (disconn_resp_to_up && lib_timer_alarm(disconn_resp_to)) {
        resend = 1;
        lib_timer_init(&disconn_resp_to, ag_getDisconnectRespTO());
    }
    if(resend) {
        pu_queue_push(to_agent, buffered_responce, strlen(buffered_responce)+1);

    }
}

static const char* prepare_sesson_details_request(char* msg, size_t size) {
    pu_log(LL_ERROR, "%s: not implemented", __FUNCTION__);
    return NULL;
}
static const char* prepare_unsucc_cam_connection(char* msg, size_t size) {
    pu_log(LL_ERROR, "%s: not implemented", __FUNCTION__);
    return NULL;

}
static const char* prepare_cam_connected(char* msg, size_t size) {
    pu_log(LL_ERROR, "%s: not implemented", __FUNCTION__);
    return NULL;
}
static const char* prepare_cam_disconnection(char* msg, size_t size) {
    pu_log(LL_ERROR, "%s: not implemented", __FUNCTION__);
    return NULL;
}
static const char* prepare_bad_cam_disconnection(char* msg, size_t size) {
    pu_log(LL_ERROR, "%s: not implemented", __FUNCTION__);
    return NULL;
}
