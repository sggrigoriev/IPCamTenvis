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
 Created by gsg on 21/10/17.
*/

#include <pthread.h>
#include <string.h>


#include "lib_http.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "ag_defaults.h"
#include "ac_http.h"
#include "at_cam_write.h"

#define PT_THREAD_NAME "CAM_WRITE"

/**********************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static volatile int stop;                           /* Thread stip flag */
static pu_queue_msg_t msg[LIB_HTTP_MAX_MSG_SIZE];   /* Buffer for sending message */

static pu_queue_t* to_camera;   /* cam_controm -> camera */
static pu_queue_t* from_camera; /* camera -> cam control */

/*******************************************************************************
 * Local functions
*/
/* Thread function */
static void* write_proc(void* params);


/*
 * Public functions implementation
 */

int at_start_cam_write() {

    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &write_proc, NULL)) return 0;
    return 1;
}

void at_stop_cam_write() {
    void *ret;

    at_set_stop_cam_write();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}

void at_set_stop_cam_write() {
    stop = 1;
}

/********************************************************************************
 * Local functions implementation
 */
static void* write_proc(void* params) {
    pu_queue_event_t events;
    to_camera = aq_get_gueue(AQ_ToCamQueue);
    stop = 0;

    events = pu_add_queue_event(pu_create_event_set(), AQ_ToCamQueue);
/* TODO: Connection procedure */
    /* Main write loop */
    while(!stop) {
        pu_queue_event_t ev;

        switch (ev = pu_wait_for_queues(events, DEFAULT_CAM_WRITE_THREAD_TO_SEC)) {
            case AQ_ToCamQueue: {
                size_t len = sizeof(msg);
                while (pu_queue_pop(to_camera, msg, &len)) {
                    int out = 0;

                    while(!out) {
                        char resp[LIB_HTTP_MAX_MSG_SIZE];
                        if (!ac_write(msg, resp, sizeof(resp))) {    /* no connection: make reconnection */
                            pu_log(LL_ERROR, "%s: Error sending. Reconnect", PT_THREAD_NAME);
                            out = ac_reconnect();

                        } else {  /* data has been written */
                            pu_log(LL_INFO, "%s: Sent to camera: %s", PT_THREAD_NAME, msg);
                            if (strlen(resp) > 0) {
                                pu_log(LL_INFO, "%s: Answer from camera forwarded to cam_control: %s", PT_THREAD_NAME, resp);
                                pu_queue_push(from_camera, resp, strlen(resp) + 1);
                            }
                        }
                    }
                 }
                 len = sizeof(msg);
             }
            break;
            case AQ_Timeout:
                break;
            case AQ_STOP:
                at_set_stop_cam_write();
                pu_log(LL_INFO, "%s received STOP event. Terminated", PT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait (to server)!", PT_THREAD_NAME, ev);
                break;
        }
    }
    pthread_exit(NULL);
}
