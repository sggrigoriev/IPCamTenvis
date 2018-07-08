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
#include <lib_timer.h>
#include <ag_cam_io/ac_alfapro.h>

#include "pu_logger.h"

#include "ab_ring_bufer.h"
#include "ac_udp.h"
#include "ac_rtsp.h"
#include "ag_settings.h"
#include "ag_defaults.h"
#include "ac_cam_types.h"
#include "at_rw_thread.h"

#define AT_THREAD_NAME "STREAMING_RW"

/********************************************
 * Local data
 */
static volatile int stop = 1;


static int is_rt_mode;
//RT socks
static t_rtsp_media_pairs rd_socks = {{-1,-1}, {-1,-1}};
static t_rtsp_media_pairs wr_socks = {{-1,-1}, {-1,-1}};

static t_ab_byte* v_rtp_buf;
static t_ab_byte* v_rtcp_buf;
static t_ab_byte* a_rtp_buf;
static t_ab_byte* a_rtcp_buf;
static size_t media_buf_size;

static pthread_t v_rtp_id;
static pthread_attr_t v_rtp_attr;
static pthread_t v_rtcp_id;
static pthread_attr_t v_rtcp_attr;

static pthread_t a_rtp_id;
static pthread_attr_t a_rtp_attr;
static pthread_t a_rtcp_id;
static pthread_attr_t a_rtcp_attr;

//Interleaved socks - they are separate just for better undestanding - no simultanious use RT and IL socks!
static int read_il_sock = -1;
static int write_il_sock = -1;

static pthread_t rdwr_id;
static pthread_attr_t rdwr_attr;

static t_ab_byte* rdwr_buf;

static t_at_rtsp_session* the_session;

pu_queue_t* fromRW;

static void thread_proc(const char* name, int read_sock, int write_sock, t_ab_byte* buf, pu_queue_t* q) {
    pu_log(LL_INFO, "%s start", name);
#ifdef RW_CYCLES
    int step = RW_CYCLES;
#endif
    lib_timer_clock_t hart_beat = {0};
    lib_timer_init(&hart_beat, 30);     /* TODO: should be taken from session timeout parameter from RTSP session */
    unsigned int bytes_passed = 0;

    char err_buf[120];
    const char* stop_msg = ao_rw_error_answer(err_buf, sizeof(err_buf));
    while(!stop) {
        t_ac_udp_read_result ret;

        if(lib_timer_alarm(hart_beat)) {
            if(!ac_alfaProOptions(the_session, 1)) {     /* hart beats from the Camera */
                pu_log(LL_ERROR, "%s: Camera is out: restart streaming process", __FUNCTION__);
                pu_queue_push(q, stop_msg, strlen(stop_msg) + 1);
                goto on_stop;
            }
            lib_timer_init(&hart_beat, 30);
            pu_log(LL_DEBUG, "%s: Bytes transferred = %d", __FUNCTION__, bytes_passed);
            bytes_passed = 0;
        }

        ret = ac_udp_read(read_sock, buf, media_buf_size, 10);

        if(!ret.rc) {            /* timeout */
            continue;
        }
        else if(ret.rc < 0) {        /* Error */
            pu_log(LL_ERROR, "%s: Lost connection to the camera for %s", AT_THREAD_NAME, name);
            pu_queue_push(q, stop_msg, strlen(stop_msg) + 1);
            goto on_stop;
        }
//        pu_log(LL_DEBUG, "%s: %d bytes read", __FUNCTION__, ret.rc);
        int rt;
        if(rt = ac_udp_write(write_sock, buf, (size_t)ret.rc), !rt) {
            pu_log(LL_ERROR, "%s: Lost connection to the %s server", AT_THREAD_NAME, name);
            pu_queue_push(q, stop_msg, strlen(stop_msg)+1);
            goto on_stop;
        }
//        pu_log(LL_DEBUG, "%s: %d bytes written", __FUNCTION__, rt);
        bytes_passed += rt;
#ifdef RW_CYCLES
        if(!step--) {
            char b[120];
            pu_log(LL_ERROR, "%s: Reach %d IOs", AT_THREAD_NAME, RW_CYCLES);
            const char* msg = ao_rw_error_answer(b, sizeof(b));
            pu_queue_push(q, msg, strlen(msg)+1);
            goto on_stop;
        }
#endif
    }
on_stop:
    pu_log(LL_INFO, "%s stop %s", AT_THREAD_NAME, name);
    sleep(1);
    pthread_exit(NULL);
}

static void* v_rtp_the_thread(void* params) {
    thread_proc("VIDEO_RTP", rd_socks.video_pair.rtp, wr_socks.video_pair.rtp, v_rtp_buf, fromRW);
    pthread_exit(NULL);
}
static void* v_rtcp_the_thread(void* params) {
    thread_proc("VIDEO_RTCP", rd_socks.video_pair.rtcp, wr_socks.video_pair.rtcp, v_rtcp_buf, fromRW);
    pthread_exit(NULL);
}

static void* a_rtp_the_thread(void* params) {
    thread_proc("AUDIO_RTP", rd_socks.audio_pair.rtp, wr_socks.audio_pair.rtp, a_rtp_buf, fromRW);
    pthread_exit(NULL);
}
static void* a_rtcp_the_thread(void* params) {
    thread_proc("AUDIO_RTCP", rd_socks.audio_pair.rtcp, wr_socks.audio_pair.rtcp, a_rtcp_buf, fromRW);
    pthread_exit(NULL);
}

static void *a_rdwr_the_thread(void* params) {
    thread_proc("INTERLEAVED_IO", read_il_sock, write_il_sock, rdwr_buf, fromRW);
    pthread_exit(NULL);
}

static int at_start_rt_rw_thread() {
    if(at_is_rw_thread_run()) return 1;
    stop = 0;
    if(pthread_attr_init(&v_rtp_attr)) goto on_error;
    if(pthread_create(&v_rtp_id, &v_rtp_attr, &v_rtp_the_thread, NULL)) goto on_error;
    if(pthread_attr_init(&v_rtcp_attr)) goto on_error;
    if(pthread_create(&v_rtcp_id, &v_rtcp_attr, &v_rtcp_the_thread, NULL)) goto on_error;

    if(pthread_attr_init(&a_rtp_attr)) goto on_error;
    if(pthread_create(&a_rtp_id, &a_rtp_attr, &a_rtp_the_thread, NULL)) goto on_error;
    if(pthread_attr_init(&a_rtcp_attr)) goto on_error;
    if(pthread_create(&a_rtcp_id, &a_rtcp_attr, &a_rtcp_the_thread, NULL)) goto on_error;

    return 1;
on_error:
    stop = 1;
    return 0;
}
static int at_start_interleaved_rw_thread() {
    if(at_is_rw_thread_run()) return 1;

    stop = 0;
    if(pthread_attr_init(&rdwr_attr)) goto on_error;
    if(pthread_create(&rdwr_id, &rdwr_attr, &a_rdwr_the_thread, NULL)) goto on_error;

    return 1;
on_error:
    stop = 1;
    return 0;
}

int at_set_rt_rw(t_rtsp_media_pairs rd, t_rtsp_media_pairs wr) {
    if(at_is_rw_thread_run()) return 1;
    fromRW = aq_get_gueue(AQ_FromRW);
    fromRW = aq_get_gueue(AQ_FromRW);
    rd_socks = rd;
    wr_socks = wr;
    is_rt_mode = 1;

    media_buf_size = ag_getStreamBufferSize();
    v_rtp_buf = NULL; v_rtcp_buf = NULL; a_rtp_buf = NULL; a_rtcp_buf = NULL;

    if(v_rtp_buf = calloc(media_buf_size, 1), !v_rtp_buf) goto on_error;
    if(v_rtcp_buf = calloc(media_buf_size, 1), !v_rtcp_buf) goto on_error;
    if(a_rtp_buf = calloc(media_buf_size, 1), !a_rtp_buf) goto on_error;
    if(a_rtcp_buf = calloc(media_buf_size, 1), !a_rtcp_buf) goto on_error;

    return 1;
on_error:
    pu_log(LL_ERROR, "%s: can't allocate the buffer at %d", AT_THREAD_NAME, __LINE__);
    if(v_rtp_buf) free(v_rtp_buf);
    if(v_rtcp_buf) free(v_rtcp_buf);
    if(a_rtp_buf) free(a_rtp_buf);
    if(a_rtcp_buf) free(a_rtcp_buf);
    v_rtp_buf = NULL; v_rtcp_buf = NULL; a_rtp_buf = NULL; a_rtcp_buf = NULL;
    return 0;
}
void at_get_rt_rw(t_rtsp_media_pairs* rd, t_rtsp_media_pairs* wr) {
    *rd = rd_socks;
    *wr = wr_socks;
}
int at_set_interleaved_rw(int rd, int wr, t_at_rtsp_session* cam_sess) {
    if(at_is_rw_thread_run()) return 1;
    fromRW = aq_get_gueue(AQ_FromRW);

    read_il_sock = rd;
    write_il_sock = wr;
    is_rt_mode = 0;

    media_buf_size = ag_getStreamBufferSize();

    if(rdwr_buf = calloc(media_buf_size, 1), !rdwr_buf) goto on_error;

    the_session = cam_sess;

    return 1;
on_error:
    pu_log(LL_ERROR, "%s: can't allocate the buffer at %d", AT_THREAD_NAME, __LINE__);
    if(rdwr_buf) free(rdwr_buf);
    rdwr_buf = NULL;
    return 0;
}
void at_get_interleaved_rw(int* rd, int* wr) {
    *rd = read_il_sock;
    *wr = write_il_sock;
}

int at_start_rw() {
    return (is_rt_mode)?at_start_rt_rw_thread():at_start_interleaved_rw_thread();
}
/*****************************
 * Stop read streaming (join)
 */
void at_stop_rw() {
    if(!at_is_rw_thread_run()) {
        pu_log(LL_WARNING, "%s is already down", AT_THREAD_NAME);
        return;
    }
    stop = 1;
    if(is_rt_mode) {
        pthread_cancel(v_rtp_id);
        pthread_cancel(v_rtcp_id);
        pthread_cancel(a_rtp_id);
        pthread_cancel(a_rtcp_id);

        pthread_attr_destroy(&v_rtp_attr);
        pthread_attr_destroy(&v_rtcp_attr);
        pthread_attr_destroy(&a_rtp_attr);
        pthread_attr_destroy(&a_rtcp_attr);

        if(v_rtp_buf) free(v_rtp_buf);
        if(v_rtcp_buf) free(v_rtcp_buf);
        if(a_rtp_buf) free(a_rtp_buf);
        if(a_rtcp_buf) free(a_rtcp_buf);
        v_rtp_buf = NULL; v_rtcp_buf = NULL; a_rtp_buf = NULL; a_rtcp_buf = NULL;
        pu_log(LL_ERROR, "%s: RW threads are down", AT_THREAD_NAME);
    }
    else {
//        pthread_cancel(rdwr_id);
        void *ret;
        pthread_join(rdwr_id, &ret);
        pthread_attr_destroy(&rdwr_attr);

//We do not close existing connections - they will be used for RTSP Teardown

        if(rdwr_buf) free(rdwr_buf);
        rdwr_buf = NULL;
        pu_log(LL_ERROR, "%s: RW thread is down", AT_THREAD_NAME);
    }
}
/*****************************
 * Check if read stream runs
 * @return 1 if runs 0 if not
 */
int at_is_rw_thread_run() {
 return !stop;
}
