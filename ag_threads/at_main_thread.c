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
#include <ag_converter/ao_cmd_data.h>

#include "pu_queue.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "ag_settings.h"

#include "ao_cmd_data.h"
#include "ao_cmd_proxy.h"

#include "at_proxy_rw.h"
#include "at_cam_video.h"


#include "at_main_thread.h"

#define AT_THREAD_NAME  "IPCamTenvis"

/****************************************************************************************
    Main thread global variables
*/
static pu_queue_msg_t mt_msg[LIB_HTTP_MAX_MSG_SIZE];    /* The only main thread's buffer! */
static pu_queue_event_t events;         /* main thread events set */
static pu_queue_t* from_poxy;           /* proxy_read -> main_thread */
static pu_queue_t* to_proxy;            /* main_thread -> proxy_write */
//static pu_queue_t* from_cam_control;    /* cam_control -> main_thread */
//static pu_queue_t* to_cam_control;      /* main_thread -> cam_control */
static pu_queue_t* to_video_mgr;        /* main_thread -> video manager */

static volatile int main_finish;        /* stop flag for main thread */

static volatile int gw_is_online;       /* 1 if there is a connection to the cloud; 0 if not */

static int video_on;                        /* 1 if video nmanager runs */

/*****************************************************************************************
    Local functions deinition
*/
static int main_thread_startup() {
    gw_is_online = 0;       /* Initial state = off */
/* Queues initiation */
    aq_init_queues();

    from_poxy = aq_get_gueue(AQ_FromProxyQueue);        /* proxy_read -> main_thread */
    to_proxy = aq_get_gueue(AQ_ToProxyQueue);           /* main_thread -> proxy_write */
//    from_cam_control = aq_get_gueue(AQ_FromCamControl); /* cam_control -> main_thread */
//    to_cam_control = aq_get_gueue(AQ_ToCamControl);     /* main_thread -> cam_control */

    to_video_mgr = aq_get_gueue(AQ_ToVideoMgr);        /* main_thread -> video manager */

    events = pu_add_queue_event(pu_create_event_set(), AQ_FromProxyQueue);
    events = pu_add_queue_event(events, AQ_FromCamControl);

/* Threads start */
    if(!at_start_proxy_rw()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", AT_THREAD_NAME, "PROXY_RW", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "PROXY_RW");

    return 1;
}
static void main_thread_shutdown() {
    at_set_stop_proxy_rw();

    if(is_video_mgr_run()) at_set_stop_video_mgr();

    at_stop_proxy_rw();
    at_stop_video_mgr();

    aq_erase_queues();
}
static void process_proxy_message(char* msg) {
    t_ao_msg data;
    t_ao_msg_type msg_type;

    switch(msg_type=ao_proxy_decode(msg, &data)) {
        case AO_IN_PROXY_ID:                         /* Stay here for comatibility with M3 Agent */
            break;
        case AO_IN_CONNECTION_STATE:                 /* save proxy device ID & auth string*/
            if(gw_is_online && data.in_connection_state.is_online) {
                ag_saveProxyID(data.in_connection_state.proxy_device_id);
                ag_saveProxyAuthToken(data.in_connection_state.proxy_auth);
                ag_saveMainURL(data.in_connection_state.main_url);
                if (!at_start_video_mgr()) {
                    video_on = 0;
                    pu_log(LL_ERROR, "%s: Video Manager start failed. No video - get clean-up your room!", AT_THREAD_NAME);
                } else {
                    video_on = 1;
                }
            }
            else {  /* Proxy is ofline */
                video_on = 0;
                at_stop_video_mgr();
                pu_log(LL_ERROR, "%s: Video Manager stop due to offline status", AT_THREAD_NAME);
            }
            break;
        case AO_IN_PZT:
            pu_log(LL_ERROR, "%s: %s PZ commands not implemented yet",AT_THREAD_NAME, msg);
            break;
        case AO_UNDEF:
            pu_log(LL_ERROR, "%s: undefined message type from Proxy. Type = %d. Message ignored", AT_THREAD_NAME, msg_type);
            break;
        default:
            pu_log(LL_ERROR, "%s: undefined message type from Proxy. Type = %d. Message ignored", AT_THREAD_NAME, msg_type);
            break;
    }
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

    unsigned int events_timeout = 0; /* Wait until the end of univerce */
    video_on = 0;
    while(!main_finish) {
        size_t len = sizeof(mt_msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;

        switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case AQ_FromProxyQueue:
                while(pu_queue_pop(from_poxy, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Proxy %s", AT_THREAD_NAME, mt_msg);
                    process_proxy_message(mt_msg);
                    len = sizeof(mt_msg);
                }
                break;
            case AQ_FromCamControl:
                pu_log(LL_ERROR, "%s: %s Camera control is not omplemented yet", AT_THREAD_NAME, mt_msg);
                break;
            case AQ_Timeout:
                break;
            case AQ_STOP:
                main_finish = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", AT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait.", AT_THREAD_NAME, ev);
                break;

        }
        if(video_on && !is_video_mgr_run()) {
            pu_log(LL_INFO, "%s Video Manager is off. Restart it.", AT_THREAD_NAME);
            video_on = at_start_video_mgr();
        }
    }
    main_thread_shutdown();
    pu_log(LL_INFO, "%s: STOP. Terminated", AT_THREAD_NAME);
    pthread_exit(NULL);
}

