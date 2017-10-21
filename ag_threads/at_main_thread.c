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


#include "lib_http.h"
#include "pu_queue.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "at_main_thread.h"

#define PT_THREAD_NAME  "IPCamTenvis"
/****************************************************************************************
    Local functione declaration
*/
static int main_thread_startup();
static void process_proxy_message(char* msg);
static void process_camera_message(char* msg);

/****************************************************************************************
    Main thread global variables
*/
static pu_queue_msg_t mt_msg[LIB_HTTP_MAX_MSG_SIZE];    /* The only main thread's buffer! */
static pu_queue_event_t events;         /* main thread events set */
static pu_queue_t* from_poxy;       /* proxy_read -> main_thread */
static pu_queue_t* to_proxy;        /* main_thread -> proxy_write */
static pu_queue_t* from_cam_control;     /* cam_control -> main_thread */
static pu_queue_t* to_cam_controle;       /* main_thread -> cam_control */

static char device_id[LIB_HTTP_DEVICE_ID_SIZE];

static volatile int main_finish;        /* stop flag for main thread */

/****************************************************************************************
    Global functions definition
*/
void at_main_thread() {
    main_finish = 0;

    if(!main_thread_startup()) {
        pu_log(LL_ERROR, "%s: Initialization failed. Abort", PT_THREAD_NAME);
        main_finish = 1;
    }

    unsigned int events_timeout = 0; /* Wait until the end of univerce */

    while(!main_finish) {
        size_t len = sizeof(mt_msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;

        switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case AQ_FromProxyQueue:
                while(pu_queue_pop(from_poxy, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Proxy %s", PT_THREAD_NAME, mt_msg);
                    process_proxy_message(mt_msg);
                    len = sizeof(mt_msg);
                }
                break;
            case AQ_FromCamControl:
                while(pu_queue_pop(from_cam_control, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Proxy %s", PT_THREAD_NAME, mt_msg);
                    process_camera_message(mt_msg);
                    len = sizeof(mt_msg);
                }
                break;
            case AQ_Timeout:
                break;
            case AQ_STOP:
                main_finish = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", PT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait.", PT_THREAD_NAME, ev);
                break;

        }
    }
}
/*****************************************************************************************
    Local functions deinition
*/

int main_thread_startup() {
    return 0;
}
static void process_proxy_message(char* msg) {

}
static void process_camera_message(char* msg) {

}