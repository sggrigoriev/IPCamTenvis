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
#include <ag_config/ag_settings.h>

#include "lib_http.h"
#include "pu_queue.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "at_proxy_rw.h"
#include "at_cam_control.h"
#include "at_cam_video.h"

#include "ao_cmd_cloud.h"
#include "ao_cma_cam.h"
#include "ac_video_interface.h"

#include "at_main_thread.h"

#define AT_THREAD_NAME  "IPCamTenvis"
/****************************************************************************************
    Local functione declaration
*/
static int main_thread_startup();
static void main_thread_shutdown();
static void process_proxy_message(char* msg);
static void process_camera_message(char* msg);

/****************************************************************************************
    Main thread global variables
*/
static pu_queue_msg_t mt_msg[LIB_HTTP_MAX_MSG_SIZE];    /* The only main thread's buffer! */
static pu_queue_event_t events;         /* main thread events set */
static pu_queue_t* from_poxy;           /* proxy_read -> main_thread */
static pu_queue_t* to_proxy;            /* main_thread -> proxy_write */
static pu_queue_t* from_cam_control;    /* cam_control -> main_thread */
static pu_queue_t* to_cam_control;     /* main_thread -> cam_control */

static volatile int main_finish;        /* stop flag for main thread */

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
                while(pu_queue_pop(from_cam_control, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Proxy %s", AT_THREAD_NAME, mt_msg);
                    process_camera_message(mt_msg);
                    len = sizeof(mt_msg);
                }
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
    }
    main_thread_shutdown();
    pu_log(LL_INFO, "%s: STOP. Terminated", AT_THREAD_NAME);
    pthread_exit(NULL);
}
/*****************************************************************************************
    Local functions deinition
*/

static int main_thread_startup() {
/* Queues initiation */
    aq_init_queues();

    from_poxy = aq_get_gueue(AQ_FromProxyQueue);        /* proxy_read -> main_thread */
    to_proxy = aq_get_gueue(AQ_ToProxyQueue);           /* main_thread -> proxy_write */
    from_cam_control = aq_get_gueue(AQ_FromCamControl); /* cam_control -> main_thread */
    to_cam_control = aq_get_gueue(AQ_ToCamControl);     /* main_thread -> cam_control */

    events = pu_add_queue_event(pu_create_event_set(), AQ_FromProxyQueue);
    events = pu_add_queue_event(events, AQ_FromCamControl);

/* Setup the ring buffer for video streaming */
    if(!ab_init(ag_getVidoeChunksAmount(), ag_getVideoChunkSize())) {
        pu_log(LL_ERROR, "%s: Videostreaming buffer allocation error");
        return 0;
    }

/* Video interface init */
    ac_video_set_io();

/* Threads start */
    if(!at_start_proxy_rw()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", AT_THREAD_NAME, "PROXY_RW", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "PROXY_RW");

    if(!at_start_cam_control()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", AT_THREAD_NAME, "CAM_CONTROL", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "CAM_CONTROL");

    return 1;
}
static void main_thread_shutdown() {
    at_set_stop_proxy_rw();
    at_set_stop_cam_control();
    at_cam_video_stop();

    at_stop_proxy_rw();
    at_stop_cam_control();

    aq_erase_queues();
    ab_close();         /* Erase videostream buffer */
}
static void process_proxy_message(char* msg) {
    t_ao_cloud_msg data;
    t_ao_cloud_msg_type msg_type;

    switch(msg_type=ao_cloud_decode(msg, &data)) {
        case AO_CLOUD_PROXY_ID:                         /* Don't know waht to do with it */
            break;
        case AO_CLOUD_CONNECTION_STATE:                 /* reconnection case - someone should send to us new conn prams*/
            at_cam_video_stop();
            break;
        case AO_CLOUD_VIDEO_START:
            at_cam_video_start(data.video_start);
            break;
        case AO_CLOUD_VIDEO_STOP:
            at_cam_video_stop();
            break;
        case AO_CLOUD_PZT:
            pu_queue_push(to_cam_control, msg, strlen(msg)+1); /* Decode/encode is in thread */
            break;
        default:
            pu_log(LL_ERROR, "%s: undefined message type from Proxy. Type = %d. Message ignored", AT_THREAD_NAME, msg_type);
            break;
    }
}

static void process_camera_message(char* msg) {
    t_ao_cam_msg data;
    t_ao_cam_msg_type msg_type;

    switch(msg_type=ao_cam_decode(msg, &data)) {
        case AO_CAM_RESULT: {
            pu_queue_msg_t answer[LIB_HTTP_MAX_MSG_SIZE];
            if(ao_cloud_encode(data, answer, sizeof(answer))) {
                pu_queue_push(to_proxy, answer, strlen(answer)+1);
            }
            else {
                pu_log(LL_ERROR, "%s: can't convert %s to cloud format, Cam message ignored", AT_THREAD_NAME, msg);
            }
        }
            break;
        default:
            pu_log(LL_ERROR, "%s: undefined message type from IPCamera. Type = %d. Message %s ignored", AT_THREAD_NAME, msg_type, msg);
            break;
    }
}