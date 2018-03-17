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
#include <ag_ring_buffer/ab_ring_bufer.h>
#include <netinet/in.h>
#include <ag_cam_io/ac_cam_types.h>
#include <ag_cam_io/ac_tcp.h>
#include <pu_queue.h>
#include <ag_converter/ao_cmd_cloud.h>
#include <string.h>
#include <ag_queues/aq_queues.h>

#include "pu_logger.h"

#include "ab_ring_bufer.h"
#include "ac_udp.h"
#include "ag_settings.h"

#include "at_cam_video_write.h"

#define AT_THREAD_NAME "VIDEO_WRITE"

/********************************************
 * Local data
 */
static volatile int stop = 1;

static t_rtsp_pair socks = {-1,-1};


static pthread_t id;
static pthread_attr_t attr;

static void* the_thread(void* params);


/*********************************************
 * Global functions
 */
/* Here src - home and destination - remote peer */
int at_start_video_write(t_rtsp_pair wr) {
    if(at_is_video_write_run()) return 1;
    socks = wr;
    stop = 0;
    if(pthread_attr_init(&attr)) {stop = 1; return 0;}
    if(pthread_create(&id, &attr, &the_thread, NULL)) {stop = 1;return 0;}

    return 1;
}

void at_stop_video_write() {
    void *ret;

    if(!at_is_video_write_run())  {
        pu_log(LL_WARNING, "%s is already down", AT_THREAD_NAME);
        return;
    }

    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    if(socks.rtp >= 0) close(socks.rtp);
    if(socks.rtcp >= 0) close(socks.rtcp);
}

int at_is_video_write_run() {
    return !stop;
}


/********************************************************
 * Local functions declaration
 * All connections are open and ready for use if we're here.
 * The goal is permanent write until the stop or connection lost
 */

static void* the_thread(void* params) {
    pu_log(LL_INFO, "%s start!", AT_THREAD_NAME);
//    struct timespec to = {0,0};
//    struct timespec rem;
    pu_queue_t* fromRW = aq_get_gueue(AQ_FromRW);

    while(!stop) {
        t_ab_block ret = ab_getBlock(0);
        if(!ret.ls_size) {
//            pu_log(LL_WARNING, "%s: Timeout to get video data", AT_THREAD_NAME);
//            nanosleep(&to, &rem);
            continue;
        }
        int sock = (ret.first)?socks.rtp:socks.rtcp;

        if(!ac_udp_write(sock, ret.data, ret.ls_size)) {
            char buf[20];
            free(ret.data);
            ret.data = NULL;
            pu_log(LL_ERROR, "%s: Lost connection to the video server", AT_THREAD_NAME);
            const char* msg = ao_rw_error_answer(buf, sizeof(buf));
            pu_queue_push(fromRW, msg, strlen(msg)+1);
            break;
        }

/*
        if(!ac_tcp_write(sock, ret.data, ret.ls_size, stop)) {
            free(ret.data);
            pu_log(LL_ERROR, "%s: Lost connection to the video server", AT_THREAD_NAME);
            break;
        }
*/
//        pu_log(LL_DEBUG, "%s: %d bytes sent to stream %d", AT_THREAD_NAME, ret.ls_size, ret.first);
        if(ret.data) free(ret.data);
    }
     pu_log(LL_INFO, "%s stop", AT_THREAD_NAME);
    pthread_exit(NULL);
}