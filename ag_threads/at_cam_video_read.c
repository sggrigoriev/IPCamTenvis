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
#include <netinet/in.h>
#include <ag_cam_io/ac_cam_types.h>

#include "pu_logger.h"

#include "ab_ring_bufer.h"
#include "ac_udp.h"
#include "ac_rtsp.h"
#include "ag_settings.h"
#include "ag_defaults.h"
#include "ac_cam_types.h"

#include "at_cam_video_read.h"


#define AT_THREAD_NAME "VIDEO_READ"

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
/* Here src - remote peer and destination - home */
int at_start_video_read(t_rtsp_pair rd) {
    if(at_is_video_read_run()) return 1;
    socks = rd;
    stop = 0;
    if(pthread_attr_init(&attr)) {stop = 1; return 0;}
    if(pthread_create(&id, &attr, &the_thread, NULL)) {stop = 1; return 0;}

    return 1;
}

void at_stop_video_read() {
    void *ret;

    if(!at_is_video_read_run()) {
        pu_log(LL_WARNING, "%s is already down", AT_THREAD_NAME);
        return;
    }

    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    if(socks.rtp >= 0) close(socks.rtp);
    if(socks.rtcp >= 0) close(socks.rtcp);
}

int at_is_video_read_run() {
    return !stop;
}

/****************************************************************
 * Local functions
 */
static void* the_thread(void* params) {
    struct timespec to = {0,0};
    struct timespec rem;
    pu_log(LL_INFO, "%s start", AT_THREAD_NAME);
    size_t buf_size = ag_getStreamBufferSize();

    while(!stop) {
        t_ab_byte* buf = malloc(ag_getStreamBufferSize());
        if(!buf) {
            pu_log(LL_ERROR, "%s: can't allocate the buffer for video read", AT_THREAD_NAME);
            goto on_stop;
        }

        t_ac_udp_read_result ret = {0,0};
        while(!stop && !ret.rc) {
            ret = ac_udp_read(socks, buf, buf_size, 10);
//            if(!sz) pu_log(LL_DEBUG, "%s: Timeout", AT_THREAD_NAME);
        }
        switch(ret.rc) {
            case 0:                        /* Timeout + stop */
            case -1:
                free(buf);
                goto on_stop;
            default:
//                pu_log(LL_DEBUG, "%s: %d bytes received from stream %d", AT_THREAD_NAME, ret.rc, ret.src);
                break;                 /* Got smth - continue processing */
        }

        t_ab_put_rc rc;
        t_ab_block blk = {ret.rc, ret.src, buf};

        rc = ab_putBlock(&blk);  // The buffer will be freed on reader size - video_write thread
        switch (rc) {
            case AB_OK:
                break;
            case AB_OVFERFLOW:
                pu_log(LL_WARNING, "%s: video buffer overflow. Some content is lost", AT_THREAD_NAME);
//                nanosleep(&to, &rem);
                break;
             default:
                pu_log(LL_ERROR, "%s unrecognized code %d received from ab_putBlock()", AT_THREAD_NAME, rc);
                break;
        }

    }
    on_stop:
        ac_close_connection(socks.rtp);
        socks.rtp = -1;
        ac_close_connection(socks.rtcp);
        socks.rtcp = -1;

    pu_log(LL_INFO, "%s stop", AT_THREAD_NAME);
    pthread_exit(NULL);
}