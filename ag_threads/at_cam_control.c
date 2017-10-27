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
#include <string.h>

#include "lib_http.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "ac_http.h"
#include "ao_json2cam.h"


#include "at_cam_control.h"

#define AT_THREAD_NAME "CAM_CONTROL"

/************************************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static volatile int stop;       /* Thread stop flag */

static pu_queue_event_t events;         /* the thread events set */
static pu_queue_t* from_cam_control;    /* cam_control -> main_thread */
static pu_queue_t* to_cam_control;      /* main_thread -> cam_control */

static pu_queue_msg_t msg[LIB_HTTP_MAX_MSG_SIZE];    /* The only main thread's buffer! */
/*****************************************************************************************
 * Local functions
 */
static void* cam_control(void* params);

/**********************************************
 * Public functione definition
 */
int at_start_cam_control() {
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &cam_control, NULL)) return 0;
     return 1;
}

void at_stop_cam_control() {
    void *ret;

    at_set_stop_cam_control();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}

void at_set_stop_cam_control() {
    stop = 1;
}
/***********************************************************/
static void* cam_control(void* params) {

    events = pu_add_queue_event(pu_create_event_set(), AQ_ToCamControl);

    from_cam_control = aq_get_gueue(AQ_FromCamControl);
    to_cam_control = aq_get_gueue(AQ_ToCamControl);

    while (!stop) {
        pu_queue_event_t ev;

        unsigned int events_timeout = 0; /* Wait until the end of univerce */

        switch (ev = pu_wait_for_queues(events, events_timeout)) {
            case AQ_ToCamControl: {
                size_t len = sizeof(msg);    /* (re)set max message lenght */
                char resp[LIB_HTTP_MAX_MSG_SIZE];
                while (pu_queue_pop(to_cam_control, msg, &len)) {
                    if(!ac_write(msg, resp, sizeof(resp))) {
                        pu_log(LL_ERROR, "%s, can't write to cam. Joppa.");
                    }
                    pu_queue_push(from_cam_control, resp, strlen(resp)+1);  /* Send answer anyway. Empty means no answer */
                }

            }
                break;
            case AQ_Timeout:
                break;
            case AQ_STOP:
                at_set_stop_cam_control();
                pu_log(LL_INFO, "%s received STOP event. Terminated", AT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait.", AT_THREAD_NAME, ev);
                break;
        }
    }
    pu_log(LL_INFO, "%s: STOP. Terminated", AT_THREAD_NAME);
    pthread_exit(NULL);
}