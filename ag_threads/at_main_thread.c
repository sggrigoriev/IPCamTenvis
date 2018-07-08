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
#include <au_string/au_string.h>


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

typedef enum {AT_ST_DISCONNECTED, AT_ST_CONNECTED, AT_ST_SIZE} t_entity_status;
typedef enum {
    AT_CMD_NOTHING,             //No command
    AT_CMD_AGENT_CONNECT,       //(Re)Connect Agent to the cloud
    AT_CMD_AGENT_DISCONNECT,    //Disconnect Agent from the cloud

    AT_CMD_WS_START,            //(Re)Start Web Socket interface (restart included)
    AT_CMD_WS_STOP,             //Stop Web Socket interface
    AT_CMD_WS_PING,             //Answer to WS ping

    AT_CMD_RW_START,            //(Re)Start streaming NB! THese commands should came from WS and/or from the cloud
    AT_CMD_RW_STOP,             //Stop streaming

    AT_CMD_SIZE
} t_agent_command;


static pu_queue_msg_t mt_msg[LIB_HTTP_MAX_MSG_SIZE];    /* The only main thread's buffer! */
static pu_queue_event_t events;         /* main thread events set */
static pu_queue_t* from_poxy;           /* proxy_read -> main_thread */
static pu_queue_t* to_wud;            /* main_thread -> wud_write  */
static pu_queue_t* from_cam;         /* cam -> main_thread */
static pu_queue_t* from_ws;        /* WS -> main_thread */
static pu_queue_t* to_proxy;        /* main_thread->proxy */
static pu_queue_t* from_stream_rw;         /* from streaming threads to main */

static volatile int main_finish;        /* stop flag for main thread */

static t_entity_status
        agent_status = AT_ST_DISCONNECTED,
        ws_status = AT_ST_DISCONNECTED,
        rw_status = AT_ST_DISCONNECTED;

static t_agent_command waiting_command = AT_CMD_NOTHING;
/*****************************************************************************************
    Local functions deinition
*/
static const char* state2text(t_entity_status state) {
    const char* txt[] = {
            "AT_ST_DISCONNECTED",
            "AT_ST_CONNECTED"
    };
    return (state < AT_ST_SIZE)?txt[state]:"Unrecognized entity state";
}
static const char* cmd2text(t_agent_command cmd) {
    const char* txt[] = {
            "AT_CMD_NOTHING",
            "AT_CMD_AGENT_CONNECT",
            "AT_CMD_AGENT_DISCONNECT",
            "AT_CMD_WS_START",
            "AT_CMD_WS_STOP",
            "AT_CMD_WS_PING",
            "AT_CMD_RW_START",
            "AT_CMD_RW_STOP"
    };
    return (cmd < AT_CMD_SIZE)?txt[cmd]:"Unrecognized cmmand!";
}
/*
 * State machines to manage all this shit with Proxy/WS/RW incoming commands
 */
static void run_rw_command(t_agent_command cmd) {
    pu_log(LL_DEBUG, "%s: Run RW command %s, RW = %s", AT_THREAD_NAME, cmd2text(cmd), state2text(rw_status));
    switch(rw_status) {
        case AT_ST_DISCONNECTED:
            switch(cmd) {
                case AT_CMD_RW_START:
                    if(ac_start_video()) {
                        ac_send_stream_confirmation();
                        rw_status = AT_ST_CONNECTED;
                        waiting_command = AT_CMD_NOTHING;
                    }
                    else {
                        waiting_command = cmd;
                        pu_log(LL_ERROR, "%s: Error RW start. RW inactive", AT_THREAD_NAME);
                    }
                    break;
                case AT_CMD_RW_STOP:
                    /* Nothing to do */
                    break;
                default:
                    pu_log(LL_ERROR, "%s: Unrecognized command = %d", AT_THREAD_NAME, cmd);
                    break;
            }
            break;
        case AT_ST_CONNECTED:
            switch(cmd) {
                case AT_CMD_RW_START:   // Restart case!
                    ac_stop_video();
                    rw_status = AT_ST_DISCONNECTED;

                    if(ac_start_video()) {
                        rw_status = AT_ST_CONNECTED;
                        waiting_command = AT_CMD_NOTHING;
                    }
                    else {
                        waiting_command = cmd;
                        pu_log(LL_ERROR, "%s: Error RW start. RW inactive", AT_THREAD_NAME);
                    }
                    break;
                case AT_CMD_RW_STOP:
                    ac_stop_video();
                    rw_status = AT_ST_DISCONNECTED;
                    break;
                default:
                    pu_log(LL_ERROR, "%s: Undecognized command for RW", AT_THREAD_NAME);
                    break;
            }
            break;
        default:
            pu_log(LL_ERROR, "%s: Unrecognized RW's status = %d", AT_THREAD_NAME, rw_status);
            break;
    }
}
static void run_ws_command(t_agent_command cmd) {
    pu_log(LL_DEBUG, "%s: Run WS command %s, WS = %s, RW = %s", AT_THREAD_NAME, cmd2text(cmd),
           state2text(ws_status), state2text(rw_status));

    switch(ws_status) {
        case AT_ST_DISCONNECTED:
            switch(cmd) {
                case AT_CMD_WS_START:
                    if(ac_connect_video()) {
                        ac_send_stream_initiation();
                        ws_status = AT_ST_CONNECTED;
                        waiting_command = AT_CMD_NOTHING;
                    }
                    else {
                        waiting_command = cmd;
                        pu_log(LL_ERROR, "%s: Error WS interface start. WS inactive", AT_THREAD_NAME);
                    }
                    break;
                case AT_CMD_WS_STOP:
                    /* Nothing to do */
                    break;
                default:
                    pu_log(LL_ERROR, "%s: Can't process command %s due to inactive WS", AT_THREAD_NAME, cmd2text(cmd));
                    break;
            }
            break;
        case AT_ST_CONNECTED:
            switch(cmd) {
                case AT_CMD_WS_START:       // Restart case!
                    run_rw_command(AT_CMD_RW_STOP);
                    ac_disconnect_video();
                    ws_status = AT_ST_DISCONNECTED;

                    if(ac_connect_video()) {
                        ac_send_stream_initiation();
                        ws_status = AT_ST_CONNECTED;
                        waiting_command = AT_CMD_NOTHING;
                     }
                    else {
                        waiting_command = cmd;
                        pu_log(LL_ERROR, "%s: Error WS interface start. WS inactive", AT_THREAD_NAME);
                    }
                    break;
                case AT_CMD_WS_STOP:
                    run_rw_command(AT_CMD_RW_STOP);
                    ac_disconnect_video();
                    ws_status = AT_ST_DISCONNECTED;
                     break;
                case AT_CMD_WS_PING:
                    at_ws_send(ao_answer_to_ws_ping());
                    break;
                default:
                    run_rw_command(cmd);
                    break;
            }
            break;
        default:
            pu_log(LL_ERROR, "%s: Unrecognized WS's status = %d", AT_THREAD_NAME, ws_status);
            break;
    }
}
static void run_command(t_agent_command cmd) {
    if(cmd == AT_CMD_NOTHING) cmd = waiting_command;

    if(cmd != AT_CMD_NOTHING) {
        pu_log(LL_DEBUG, "%s: Run command %s, AGENT = %s, WS = %s, RW = %s", AT_THREAD_NAME, cmd2text(cmd),
               state2text(agent_status), state2text(ws_status), state2text(rw_status));
    }

    switch (agent_status) {
        case AT_ST_DISCONNECTED:
            switch(cmd) {
                case AT_CMD_AGENT_CONNECT:
                    agent_status = AT_ST_CONNECTED;
                    run_ws_command(AT_CMD_WS_START);
                    break;
                case AT_CMD_AGENT_DISCONNECT:
                case AT_CMD_NOTHING:
                    /* Do nothing */
                    break;
                default:    // Commangs for child entities
                    pu_log(LL_ERROR, "%s: Can't process command %s dueto disconnected Agent", AT_THREAD_NAME, cmd2text(cmd));
                    break;
            }
            break;
        case AT_ST_CONNECTED:
            switch(cmd) {
                case AT_CMD_AGENT_CONNECT:      // Reconnect case
                    run_ws_command(AT_CMD_WS_STOP);
                    run_ws_command(AT_CMD_WS_START);
                    break;
                case AT_CMD_AGENT_DISCONNECT:
                    run_ws_command(AT_CMD_WS_STOP);
                    agent_status = AT_ST_DISCONNECTED;
                    break;
                case AT_CMD_NOTHING:
                    /* Do nothing */
                    break;
                default:
                    run_ws_command(cmd);
                    break;
            }
            break;
        default:
            pu_log(LL_ERROR, "%s: Unrecognized Agent's status = %d", AT_THREAD_NAME, agent_status);
    }
}

static void send_wd() {
        char buf[LIB_HTTP_MAX_MSG_SIZE];

        pr_make_wd_alert4WUD(buf, sizeof(buf), ag_getAgentName(), ag_getProxyID());
        pu_queue_push(to_wud, buf, strlen(buf)+1);
}

static t_agent_command process_proxy_message(char* msg) {
    t_ao_msg data;
    t_ao_msg_type msg_type;
    t_agent_command ret = AT_CMD_NOTHING;

    switch(msg_type=ao_proxy_decode(msg, &data)) {
        case AO_IN_PROXY_ID:                         /* Stays here for comatibility with M3 Agent */
            break;
        case AO_IN_CONNECTION_INFO: {
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
                ret = AT_CMD_AGENT_CONNECT;
        }
            break;
        case AO_IN_MANAGE_VIDEO: {
            char buf[128];
            ret = (data.in_manage_video.start_it) ? AT_CMD_RW_START : AT_CMD_RW_STOP;
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
static t_agent_command process_ws_message(char* msg) {
    t_ao_msg data;
    t_ao_msg_type msg_type;
    t_agent_command ret = AT_CMD_NOTHING;

    if(msg_type = ao_cloud_decode(msg, &data), msg_type != AO_WS_ANSWER) {
        pu_log(LL_ERROR, "%s: Can't process message from Web Socket. Message type = %d. Message ignored", AT_THREAD_NAME, msg_type);
        return ret;
    }
    switch(data.ws_answer.ws_msg_type) {
        case AO_WS_PING:
            ret = AT_CMD_WS_PING;
            break;
        case AO_WS_ABOUT_STREAMING: {
            unsigned int old_viewers_amount = at_ws_get_active_viewers_amount();
            unsigned int new_viewers_amount = (data.ws_answer.viwers_count < 0) ?
                                              old_viewers_amount + data.ws_answer.viewers_delta :
                                              (unsigned int) data.ws_answer.viwers_count;

            if ((data.ws_answer.is_start) || ((new_viewers_amount != 0) && (old_viewers_amount == 0))) {
                pu_log(LL_INFO, "%s: Sart streaming requested by Web Socket.", AT_THREAD_NAME);
                ret = (rw_status == AT_ST_CONNECTED)?AT_CMD_NOTHING:AT_CMD_RW_START;
            } else if ((new_viewers_amount == 0) && (old_viewers_amount != 0)) {
                pu_log(LL_INFO, "%s: Stop streaming requested by Web Socket - no connected viewers", AT_THREAD_NAME);
                ret = AT_CMD_RW_STOP;
            }
            at_ws_set_active_viewers_amount(new_viewers_amount);
            pu_log(LL_DEBUG, "%s: Active viwers amount: %d", AT_THREAD_NAME, new_viewers_amount);
        }
            break;
        case AO_WS_ERROR:
            pu_log(LL_ERROR, "%s: Error from Web Socket. RC = %d %s. Reconnect", AT_THREAD_NAME,
                   data.ws_answer.rc, ao_ws_error(data.ws_answer.rc));
            ret = AT_CMD_WS_START;
            break;
        default:
            pu_log(LL_INFO, "%s: Unrecognized message type %d from Web Socket. Ignored", AT_THREAD_NAME, data.ws_answer.ws_msg_type);
            break;
    }
    return ret;
}
static t_agent_command process_rw_message(char* msg) {
    t_ao_msg data;
    t_ao_msg_type msg_type;
    t_agent_command ret = AT_CMD_NOTHING;

    if((msg_type = ao_cloud_decode(msg, &data), msg_type != AO_WS_ANSWER) || (data.ws_answer.ws_msg_type != AO_WS_ERROR)) {
        pu_log(LL_ERROR, "%s: Can't process message from streaming R/W treads. Message type = %d. Message ignored", AT_THREAD_NAME, msg_type);
        return ret;
    }
    pu_log(LL_ERROR, "%s: Error from streaming R/W treads. RC = %d %s. Reconnect", AT_THREAD_NAME,
           data.ws_answer.rc, ao_ws_error(data.ws_answer.rc));
    ret = (rw_status = AT_ST_CONNECTED)?AT_CMD_RW_START:AT_CMD_NOTHING;
    return ret;
}

static int main_thread_startup() {

    aq_init_queues();

    from_poxy = aq_get_gueue(AQ_FromProxyQueue);        /* proxy_read -> main_thread */
    to_proxy = aq_get_gueue(AQ_ToProxyQueue);           /* main_thred -> proxy_write */
    to_wud = aq_get_gueue(AQ_ToWUD);                    /* main_thread -> proxy_write */
    from_cam = aq_get_gueue(AQ_FromCam);                /* cam_control -> main_thread */
    from_ws = aq_get_gueue(AQ_FromWS);                  /* WS -> main_thread */
    from_stream_rw = aq_get_gueue(AQ_FromRW);           /* Streaming RW tread(s) -> main */

    events = pu_add_queue_event(pu_create_event_set(), AQ_FromProxyQueue);
    events = pu_add_queue_event(events, AQ_FromCam);
    events = pu_add_queue_event(events, AQ_FromWS);
    events = pu_add_queue_event(events, AQ_FromRW);

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
/****************************************************************************************
    Global function definition
*/
void at_main_thread() {
    main_finish = 0;

    if(!main_thread_startup()) {
        pu_log(LL_ERROR, "%s: Initialization failed. Abort", AT_THREAD_NAME);
        main_finish = 1;
    }

    lib_timer_clock_t wd_clock = {0};           /* timer for watchdog sending */
    lib_timer_init(&wd_clock, ag_getAgentWDTO());   /* Initiating the timer for watchdog sendings */

    unsigned int events_timeout = 1; /* Wait until the end of universe */

    while(!main_finish) {
        size_t len = sizeof(mt_msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;

         switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case AQ_FromProxyQueue:
                while(pu_queue_pop(from_poxy, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Proxy %s", AT_THREAD_NAME, mt_msg);
                    run_command(process_proxy_message(mt_msg));
                    len = sizeof(mt_msg);
                 }
                 break;
            case AQ_FromWS:
                while(pu_queue_pop(from_ws, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Web Socket interface %s, len = %d", AT_THREAD_NAME, mt_msg, len);
                    run_command(process_ws_message(mt_msg));
                    len = sizeof(mt_msg);
                }
                break;
            case AQ_FromRW:
                while(pu_queue_pop(from_stream_rw, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the streaming threads %s", AT_THREAD_NAME, mt_msg);
                    run_command(process_rw_message(mt_msg));
                    len = sizeof(mt_msg);
                }
                break;
            case AQ_FromCam:
                pu_log(LL_ERROR, "%s: %s Camera async interface not implemented yet", AT_THREAD_NAME, mt_msg);
                break;
            case AQ_Timeout:
                if(waiting_command != AT_CMD_NOTHING) {
                    run_command(AT_CMD_NOTHING);    /* waiting command will be processed inside */
                }
//                pu_log(LL_DEBUG, "%s: timeout", AT_THREAD_NAME);
                break;
            case AQ_STOP:
                main_finish = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", AT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait. Message = %s", AT_THREAD_NAME, ev, mt_msg);
                 break;
        }
         /* Place for own periodic actions */
        /*1. Wathchdog */
        if(lib_timer_alarm(wd_clock)) {
            send_wd();
            lib_timer_init(&wd_clock, ag_getAgentWDTO());
        }
    }
    main_thread_shutdown();
    pu_log(LL_INFO, "%s: STOP. Terminated", AT_THREAD_NAME);
    pthread_exit(NULL);
}
