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
 Read the video flow from the camera
*/
#include <pthread.h>
#include <malloc.h>

#include "pu_logger.h"

#include "ab_ring_bufer.h"
#include "ac_video_interface.h"

#include "at_cam_video_read.h"


#define AT_THREAD_NAME "VIDEO_READ"

/********************************************
 * Local data
 */
static volatile int stop = 0;
static volatile int stopped = 1;

static volatile int reconnect;

static pthread_t id;
static pthread_attr_t attr;

static void* the_thread(void* params);

static t_ao_video_start conn_params;

/*********************************************
 * Global functions
 */

int at_start_video_read(t_ao_video_start params) {
    conn_params = params;
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &the_thread, NULL)) return 0;
    stopped = 0;
    return 1;
}

void at_stop_video_read() {
    void *ret;

    if(stopped) return;

    at_set_stop_video_read();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
    stopped = 1;
}

int at_is_video_read_run() {
    return !stopped;
}

void at_set_stop_video_read() {
    stop = 1;
}

/****************************************************************
 * Local functions
 */
static void* the_thread(void* params) {
    pu_log(LL_INFO, "%s start", AT_THREAD_NAME);
    while(!stop) {
        if (!ac_init_connections(conn_params, AC_READ_CONN)) {
            pu_log(LL_ERROR, "%s: can not establish video streaming.", AT_THREAD_NAME);
            reconnect = 1;
            sleep(1);
        }
        else {
            reconnect = 0;
        }
        while (!stop && !reconnect) {
            t_ab_byte* buf = malloc(AC_MAX_STREAM_BUFF_SIZE);
            if(!buf) {
                pu_log(LL_ERROR, "%s: can't allocate the buffer for video read", AT_THREAD_NAME);
                sleep(1);
                continue;
            }
            size_t sz = ac_video_read(AC_MAX_STREAM_BUFF_SIZE, buf);
            if(!sz) {
                pu_log(LL_ERROR, "%s: Lost connection to the video source", AT_THREAD_NAME);
                ac_close_connections(AC_READ_CONN);
                reconnect = 1;
            }
            else {
                t_ab_put_rc rc = ab_putBlock(sz, buf);
                switch (rc) {
                    case AB_OK:
                        break;
                    case AB_OVFERFLOW:
                        pu_log(LL_WARNING, "%s: ideo buffer overflow. Some content is lost", AT_THREAD_NAME);
                        break;
                    default:
                        pu_log(LL_ERROR, "%s unrecognized code %d received from ab_putBlock()", AT_THREAD_NAME, rc);
                         break;
                }

            }
            free(buf);
        }
    }
    ac_close_connections(AC_READ_CONN);
    pu_log(LL_INFO, "%s stop", AT_THREAD_NAME);
    pthread_exit(NULL);
}