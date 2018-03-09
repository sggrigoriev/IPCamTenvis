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

#include <memory.h>
#include <errno.h>
#include <ag_converter/ao_cmd_cloud.h>
#include <ag_converter/ao_cmd_data.h>


#include "lib_timer.h"
#include "pu_queue.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "ag_settings.h"

#include "ao_cmd_data.h"
#include "ao_cmd_proxy.h"
#include "pr_commands.h"

#include "at_proxy_rw.h"
#include "at_wud_write.h"
#include "ac_video.h"

#include "at_main_thread.h"
#include "at_ws.h"

#define AT_THREAD_NAME  "IPCamTenvis"

/****************************************************************************************
    Main thread global variables
*/

typedef enum {AT_DISCONNECTED, AT_CONNECTED, AT_STREAMING} t_agent_status;
typedef enum {AT_WH_NOTHING, AT_WH_CONN_CHANGED, AT_WH_VSTART_REQUESTED, AT_WH_VSTOP_REQUESTED, AT_WS_RESTART_REQUESTED} t_agen_what_happened;
typedef enum {AT_NO_SRC, AT_SRC_CLOUD, AT_SRC_WS, AT_SRC_CAM} t_agent_source;

static pu_queue_msg_t mt_msg[LIB_HTTP_MAX_MSG_SIZE];    /* The only main thread's buffer! */
static pu_queue_event_t events;         /* main thread events set */
static pu_queue_t* from_poxy;           /* proxy_read -> main_thread */
static pu_queue_t* to_wud;            /* main_thread -> wud_write  */
static pu_queue_t* from_cam;         /* cam -> main_thread */
static pu_queue_t* from_ws;        /* WS -> main_thread */
static pu_queue_t* to_proxy;        /* main_thread->proxy */

static volatile int main_finish;        /* stop flag for main thread */


/*****************************************************************************************
    Local functions deinition
*/
static void send_wd() {
        char buf[LIB_HTTP_MAX_MSG_SIZE];

        pr_make_wd_alert4WUD(buf, sizeof(buf), ag_getAgentName(), ag_getProxyID());
        pu_queue_push(to_wud, buf, strlen(buf)+1);
}


static int main_thread_startup() {

    aq_init_queues();

    from_poxy = aq_get_gueue(AQ_FromProxyQueue);        /* proxy_read -> main_thread */
    to_proxy = aq_get_gueue(AQ_ToProxyQueue);           /* main_thred -> proxy_write */
    to_wud = aq_get_gueue(AQ_ToWUD);                    /* main_thread -> proxy_write */
    from_cam = aq_get_gueue(AQ_FromCam);                /* cam_control -> main_thread */
    from_ws = aq_get_gueue(AQ_FromWS);                  /* WS -> main_thread */

    events = pu_add_queue_event(pu_create_event_set(), AQ_FromProxyQueue);
    events = pu_add_queue_event(events, AQ_FromCam);
    events = pu_add_queue_event(events, AQ_FromWS);

/* Threads start */
    if(!at_start_proxy_rw()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", AT_THREAD_NAME, "PROXY_RW", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "PROXY_RW");

    if(!at_start_wud_write()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", AT_THREAD_NAME, "WUD_WRITE", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "WUD_WRITE");

    return 1;
}
static void main_thread_shutdown() {
    ac_stop_video();
    ac_disconnect_video();

    at_set_stop_proxy_rw();
    at_set_stop_wud_write();

    at_stop_wud_write();
    at_stop_proxy_rw();

    aq_erase_queues();
}

static t_agen_what_happened process_proxy_message(char* msg) {
    t_ao_msg data;
    t_ao_msg_type msg_type;
    t_agen_what_happened ret = AT_WH_NOTHING;

    switch(msg_type=ao_proxy_decode(msg, &data)) {
        case AO_IN_PROXY_ID:                         /* Stays here for comatibility with M3 Agent */
            break;
        case AO_IN_CONNECTION_STATE: {
            int info_changed = 0;

            if(data.in_connection_state.is_online) {
                 if(strcmp(ag_getProxyID(), data.in_connection_state.proxy_device_id) != 0) {
                    pu_log(LL_INFO, "%s:Proxy sent new ProxyID. Old one = %s, New one = %s.", AT_THREAD_NAME, ag_getProxyID(), data.in_connection_state.proxy_device_id);
                    ag_saveProxyID(data.in_connection_state.proxy_device_id);
                    info_changed = 1;
                }
                if(strcmp(ag_getProxyAuthToken(), data.in_connection_state.proxy_auth) != 0) {
                    pu_log(LL_INFO, "%s:Proxy sent new Auth token. Old one = %s, New one = %s.", AT_THREAD_NAME, ag_getProxyAuthToken(), data.in_connection_state.proxy_auth);
                    ag_saveProxyAuthToken(data.in_connection_state.proxy_auth);
                    info_changed = 1;
                }
                if(strcmp(ag_getMainURL(), data.in_connection_state.main_url) != 0) {
                    pu_log(LL_INFO, "%s:Proxy sent new Main URL. Old one = %s, New one = %s.", AT_THREAD_NAME, ag_getMainURL(), data.in_connection_state.main_url);
                    ag_saveMainURL(data.in_connection_state.main_url);
                    info_changed = 1;
                }
            }
            if(info_changed)
                ret = AT_WH_CONN_CHANGED;
        }
            break;
        case AO_IN_MANAGE_VIDEO: {
            char buf[128];
            ret = (data.in_manage_video.start_it) ? AT_WH_VSTART_REQUESTED : AT_WH_VSTOP_REQUESTED;
            ao_answer_to_command(buf, sizeof(buf), data.in_manage_video.command_id, 0);
            pu_queue_push(to_proxy, buf, strlen(buf) + 1);
        }
            break;
        default:
            pu_log(LL_ERROR, "%s: Can't process message from Proxy. Message type = %d. Message ignored", AT_THREAD_NAME, msg_type);
            break;
    }
    return ret;
}
static t_agen_what_happened process_ws_message(char* msg) {
    t_ao_msg data;
    t_ao_msg_type msg_type;
    t_agen_what_happened ret = AT_WH_NOTHING;

    if(msg_type = ao_cloud_decode(msg, &data), msg_type != AO_WS_ANSWER) {
        pu_log(LL_ERROR, "%s: Can't process message from Web Spcket. Message type = %d. Message ignored", AT_THREAD_NAME, msg_type);
        return ret;
    }
    switch (data.ws_answer.ws_msg_type) {
        case AO_WS_START:
            pu_log(LL_INFO, "%s: Start streaming requested by Web Socket", AT_THREAD_NAME);
            ret = AT_WH_VSTART_REQUESTED;
            break;
        case AO_WS_STOP:
            pu_log(LL_INFO, "%s: Stop streaming requested by Web Socket", AT_THREAD_NAME);
            ret = AT_WH_VSTOP_REQUESTED;
            break;
        case AO_WS_ERROR:
            pu_log(LL_ERROR, "%s: Error from Web Socket. RC = %d %s. Message ignored", AT_THREAD_NAME, data.ws_answer.rc, ao_ws_error(data.ws_answer.rc));
            ret = AT_WS_RESTART_REQUESTED;
            break;
        case AO_WS_NOT_INTERESTING:
            pu_log(LL_INFO, "%s: Message %s from Web Socket ignored", AT_THREAD_NAME, msg);
            ret = AT_WH_NOTHING;
            break;
        default:
            pu_log(LL_INFO, "%s: Unrecognized message type %d from Web Socket", AT_THREAD_NAME, data.ws_answer.ws_msg_type);
            ret = AT_WH_VSTOP_REQUESTED;
            break;
    }
    return ret;
}

static t_agent_status proceed_action(t_agen_what_happened wh, t_agent_status st, t_agent_source src) {
    if(wh == AT_WH_NOTHING) return st;  //Reaction on timeout and other ationless iterations

    switch (st) {
        case AT_DISCONNECTED:
            if(wh == AT_WH_CONN_CHANGED) {  /* make all connection procedures */
                if(!ac_connect_video()) {
                    pu_log(LL_ERROR, "%s: Error video initiating", AT_THREAD_NAME);
                }
                else {
                    ac_send_stream_initiation();
                    st = AT_CONNECTED;
                }
            }
            break;
        case AT_CONNECTED:
            switch (wh) {
                case AT_WH_CONN_CHANGED:
                case AT_WS_RESTART_REQUESTED:
                    ac_disconnect_video();
                    if(!ac_connect_video()) {
                        pu_log(LL_ERROR, "%s: Error video reconnect", AT_THREAD_NAME);
                        st = AT_DISCONNECTED;
                    }
                    else {
                        ac_send_stream_initiation();
                        st = AT_CONNECTED;
                    }
                    break;
                case AT_WH_VSTART_REQUESTED:
                    if(!ac_start_video()) {
                        pu_log(LL_ERROR, "%s: Error video start", AT_THREAD_NAME);
                    }
                    else {
                        if(src == AT_SRC_WS) ac_send_stream_confirmation();     //Ask ws thread to send streaming confirmation to the WS
                        st = AT_STREAMING;
                    }
                    break;
                default:
                    pu_log(LL_ERROR, "%s: Action %d not supported in 'CONNECTED' status", AT_THREAD_NAME);
                    break;
            }
            break;
        case AT_STREAMING:
            switch (wh) {
                case AT_WH_CONN_CHANGED:
                case AT_WS_RESTART_REQUESTED:
                    ac_stop_video();
                    ac_disconnect_video();
                    if(!ac_connect_video()) {
                        pu_log(LL_ERROR, "%s: Error video reconnect", AT_THREAD_NAME);
                        st = AT_DISCONNECTED;
                    }
                    else {
                        ac_send_stream_initiation();
                        st = AT_CONNECTED;
                    }
                    break;
                case AT_WH_VSTART_REQUESTED:    /* We are on streaming already. So we need just to send the confirmation */
                    if(src == AT_SRC_WS) ac_send_stream_confirmation();
                    break;
                case AT_WH_VSTOP_REQUESTED:
                    ac_stop_video();
                    st = AT_CONNECTED;
                    break;
                default:
                    pu_log(LL_ERROR, "%s: Action %d not supported in 'STREAMING' status", AT_THREAD_NAME);
                    break;
            }
            if(!ac_streaming_run()) { /* streaming restart required! */
                ac_stop_video();
                st = AT_CONNECTED;
            }
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported Agent status %d", AT_THREAD_NAME, st);
            break;
    }
    return st;
}

/****************************************************************************************
    Global function definition
*/
void at_main_thread() {
    t_agent_status status = AT_DISCONNECTED;
    t_agen_what_happened wtf = AT_WH_NOTHING;


    main_finish = 0;

        if(!main_thread_startup()) {
        pu_log(LL_ERROR, "%s: Initialization failed. Abort", AT_THREAD_NAME);
        main_finish = 1;
    }

    lib_timer_clock_t wd_clock = {0};           /* timer for watchdog sending */
    lib_timer_init(&wd_clock, ag_getAgentWDTO());   /* Initiating the timer for watchdog sendings */

    unsigned int events_timeout = 1; /* Wait until the end of univerce */

    while(!main_finish) {
        size_t len = sizeof(mt_msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;
        t_agent_source src = AT_NO_SRC;

        switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case AQ_FromProxyQueue:
                if(pu_queue_pop(from_poxy, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Proxy %s", AT_THREAD_NAME, mt_msg);
                    wtf = process_proxy_message(mt_msg);
                }
                else {
                    pu_log(LL_ERROR, "%s: Empty message from the Proxy! Ignored.", AT_THREAD_NAME);
                }
                src = AT_SRC_CLOUD;
                break;
            case AQ_FromWS:
                if(pu_queue_pop(from_ws, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Web Socket interface %s", AT_THREAD_NAME, mt_msg);
                    wtf = process_ws_message(mt_msg);
                }
                src = AT_SRC_WS;
                break;
            case AQ_FromCam:
                pu_log(LL_ERROR, "%s: %s Camera async interface not omplemented yet", AT_THREAD_NAME, mt_msg);
                src = AT_SRC_CAM;
                break;
            case AQ_Timeout:
//                pu_log(LL_DEBUG, "%s: timeout", AT_THREAD_NAME);
                src = AT_NO_SRC;
                break;
            case AQ_STOP:
                main_finish = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", AT_THREAD_NAME);
                src = AT_NO_SRC;
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait.", AT_THREAD_NAME, ev);
                src = AT_NO_SRC;
                break;

        }
         /* Place for own periodic actions */
        /*1. Wathchdog */
        if(lib_timer_alarm(wd_clock)) {
            send_wd();
            lib_timer_init(&wd_clock, ag_getAgentWDTO());
        }
        /*2. Processing status changes */
        status = proceed_action(wtf, status, src);
        wtf = AT_WH_NOTHING;                    // Reset action before the next iteration
    }
    main_thread_shutdown();
    pu_log(LL_INFO, "%s: STOP. Terminated", AT_THREAD_NAME);
    pthread_exit(NULL);
}

