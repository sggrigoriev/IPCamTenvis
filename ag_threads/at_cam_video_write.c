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
 Created by gsg on 30/10/17.
*/
#include <pthread.h>
#include <malloc.h>

#include "pu_logger.h"

#include "ab_ring_bufer.h"
#include "ac_video_interface.h"

#include "at_cam_video_write.h"

#define AT_THREAD_NAME "VIDEO_WRITE"

/********************************************
 * Local data
 */
static volatile int stop = 0;
static volatile int stopped = 1;

static volatile int reconnect;

static pthread_t id;
static pthread_attr_t attr;

static void* the_thread(void* params);


/*********************************************
 * Global functions
 */

int at_start_video_write() {
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &the_thread, NULL)) return 0;
    stopped = 0;
    return 1;
}

void at_stop_video_write() {
    void *ret;

    if(stopped) return;

    at_set_stop_video_write();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
    stopped = 1;
}

int at_is_video_write_run() {
    return !stopped;
}
void at_set_stop_video_write() {
    stop = 1;
}

/********************************************************
 * Local functions declaration
 * All connections are open and ready for use if we're here.
 * The goal is permanent write until the stop or commection lost
 */

static void* the_thread(void* params) {
    pu_log(LL_INFO, "%s start", AT_THREAD_NAME);
    while(!stop) {
        if (!ac_init_connections(conn_params, AC_WRITE_CONN)) {
            pu_log(LL_ERROR, "%s: can not establish video streaming.", AT_THREAD_NAME);
            reconnect = 1;
            sleep(1);
        }
        else {
            reconnect = 0;
        }
        while (!stop && !reconnect) {
            t_ab_block ret = ab_getBlock(20);
            if(!ret.ls_size) {
                pu_log(LL_WARNING, "%s: Timeout to get video data", AT_THREAD_NAME);
                continue;
            }
            if (!ac_video_write(ret.ls_size, ret.data)) {
                pu_log(LL_ERROR, "%s: Lost connection to the video-client", AT_THREAD_NAME);
                ac_close_connections(AC_WRITE_CONN);
                reconnect = 1;
            }
            free(ret.data);
        }
    }
    ac_close_connections(AC_WRITE_CONN);
    pu_log(LL_INFO, "%s stop", AT_THREAD_NAME);
    pthread_exit(NULL);
}