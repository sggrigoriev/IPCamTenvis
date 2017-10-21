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
#include "at_cam_read.h"

#define PT_THREAD_NAME "CAM_READ"

/************************************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static volatile int stop;       /* Thread stop flag */

static pu_queue_t* from_camera;     /* cam_control -> main_thread */

/*****************************************************************************************
 * Local functions
 */
/* Thread function */
static void* read_proc(void* params);

/* Get the message from the cloud
 *  buf     - buffer for message received
 *  size    - buffer size
 */
static void read_from_camera(char* buf, size_t size);

/************************************************************************************************
 * Public functions impementation
 */
int at_start_cam_read() {
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &read_proc, NULL)) return 0;
    return 1;
}

void at_stop_cam_read() {
    void *ret;

    at_set_stop_cam_read();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}

void at_set_stop_cam_read() {
    stop = 1;
}

static void* read_proc(void* params) {
    from_camera = aq_get_gueue(AQ_FromCamQueue);

    stop = 0;

    char buf[LIB_HTTP_MAX_MSG_SIZE];

/* Main read loop */
    while(!stop) {
        read_from_camera(buf, sizeof(buf));
        pu_log(LL_DEBUG, "%s: received from camera: %s", PT_THREAD_NAME, buf);

        pu_queue_push(from_camera, buf, strlen(buf)+1); /* Forward the message ot the proxy_main */
    }
    pu_log(LL_INFO, "%s: STOP. Terminated", PT_THREAD_NAME);
    pthread_exit(NULL);
}
