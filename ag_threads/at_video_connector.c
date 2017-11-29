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
 Created by gsg on 23/11/17.
*/
#include <pthread.h>
#include <memory.h>

#include "pu_logger.h"
#include "pu_queue.h"

#include "at_cam_video_read.h"
#include "at_cam_video_write.h"

#include "at_video_connector.h"


/*************************************************************************
 * Local data & functione
 */
#define AT_THREAD_NAME "VIDEO_CONNECTOR"

static pthread_t id;
static pthread_attr_t attr;

static volatile int stop;       /* Thread stop flag */

static void* main_thread(void* params);

static void stop_streaming();
static int video_server_connect();
static int cam_connect();
static void say_bad_connection_to_vm();

/*************************************************************************
 * Global functions definition
 */
int at_start_video_connector() {
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &main_thread, NULL)) return 0;
    return 1;
}
int at_stop_video_connector() {
    void* ret;

    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    return 1;
}

/*************************************************************************
 * Local functionsimplementation
 */

static void* main_thread(void* params) {
    int video_sock = -1;
    int cam_sock = -1;
    char request[LIB_HTTP_MAX_MSG_SIZE] = {0};
    char answer[LIB_HTTP_MAX_MSG_SIZE] = {0};

    stop = 0;

    on_reconnect:
        video_sock = video_server_connect();
        cam_sock = cam_connect();
        if((video_sock < 0) || (cam_sock < 0)) {
            say_bad_connection_to_vm();
            stop = 0;
        }
    while(!stop) {

    }

/* total shutdoen */
    stop_streaming();
    pthread_exit(NULL);
}

static void stop_streaming() {
    at_set_stop_video_read();
    at_set_stop_video_write();

    at_stop_video_read();
    at_stop_video_write();
}