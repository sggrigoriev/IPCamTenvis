/*
 *  Copyright 2018 People Power Company
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
 Created by gsg on 17/03/18.
*/

#include <pthread.h>
#include <malloc.h>
#include <netinet/in.h>
#include <ag_cam_io/ac_cam_types.h>
#include <pu_queue.h>
#include <ag_queues/aq_queues.h>
#include <string.h>
#include <ag_converter/ao_cmd_cloud.h>
#include <ag_cam_io/ac_udp.h>

#include "pu_logger.h"

#include "ab_ring_bufer.h"
#include "ac_udp.h"
#include "ac_rtsp.h"
#include "ag_settings.h"
#include "ag_defaults.h"
#include "ac_cam_types.h"
#include "at_rw_thread.h"

#define AT_THREAD_NAME "VIDEO_RW"

/********************************************
 * Local data
 */
static volatile int stop = 1;
static t_rtsp_pair rd_socks = {-1,-1};
static t_rtsp_pair wr_socks = {-1,-1};

static pthread_t id;
static pthread_attr_t attr;

static void* the_thread(void* params) {
    pu_log(LL_INFO, "%s start", AT_THREAD_NAME);

    pu_queue_t* fromRW = aq_get_gueue(AQ_FromRW);

    size_t buf_size = ag_getStreamBufferSize();
    t_ab_byte* buf = malloc(ag_getStreamBufferSize());
    if(!buf) {
        pu_log(LL_ERROR, "%s: can't allocate the buffer for video read", AT_THREAD_NAME);
        goto on_stop;
    }

    while(!stop) {
        t_ac_udp_read_result ret;
        ret = ac_udp_read(rd_socks, buf, buf_size, 10);
        if(ret.rc > 0) {
            int sock = (ret.src)?wr_socks.rtp:wr_socks.rtcp;
            if(!ac_udp_write(sock, buf, (size_t)ret.rc)) {
                char b[20];
                pu_log(LL_ERROR, "%s: Lost connection to the video server", AT_THREAD_NAME);
                const char* msg = ao_rw_error_answer(b, sizeof(b));
                pu_queue_push(fromRW, msg, strlen(msg)+1);
                goto on_stop;
            }
        }
        else {
            char err_buf[20];
            pu_log(LL_ERROR, "%s: Lost connection to the camera", AT_THREAD_NAME);
            const char* msg = ao_rw_error_answer(err_buf, sizeof(buf));
            pu_queue_push(fromRW, msg, strlen(msg)+1);
            goto on_stop;
        }
    }
on_stop:
    ac_close_connection(rd_socks.rtp);
    rd_socks.rtp = -1;
    ac_close_connection(rd_socks.rtcp);
    rd_socks.rtcp = -1;

    ac_close_connection(wr_socks.rtp);
    wr_socks.rtp = -1;
    ac_close_connection(wr_socks.rtcp);
    wr_socks.rtcp = -1;

    if(buf) free(buf);

    pu_log(LL_INFO, "%s stop", AT_THREAD_NAME);
    pthread_exit(NULL);
}

/***************************
 * Start getting cideo stream from the camera
 * @return - 1 is OK, 0 if not
 */
int at_start_rw_thread(t_rtsp_pair rd, t_rtsp_pair wr) {
    if(at_is_rw_thread_run()) return 1;
    rd_socks = rd;
    wr_socks = wr;
    stop = 0;
    if(pthread_attr_init(&attr)) {stop = 1; return 0;}
    if(pthread_create(&id, &attr, &the_thread, NULL)) {stop = 1; return 0;}

    return 1;
}
/*****************************
 * Stop read streaming (join)
 */
void at_stop_rw_thread() {
    void *ret;

    if(!at_is_rw_thread_run()) {
        pu_log(LL_WARNING, "%s is already down", AT_THREAD_NAME);
        return;
    }

    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    if(rd_socks.rtp >= 0) close(rd_socks.rtp);
    if(rd_socks.rtcp >= 0) close(rd_socks.rtcp);
    if(wr_socks.rtp >= 0) close(wr_socks.rtp);
    if(wr_socks.rtcp >= 0) close(wr_socks.rtcp);
}
/*****************************
 * Check if read stream runs
 * @return 1 if runs 0 if not
 */
int at_is_rw_thread_run() {
 return !stop;
}
