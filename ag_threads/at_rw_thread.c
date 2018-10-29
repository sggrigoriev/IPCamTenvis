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
#include <string.h>

#include "pu_logger.h"
#include "pu_queue.h"
#include "lib_timer.h"

#include "aq_queues.h"
#include "ao_cmd_cloud.h"
#include "ac_udp.h"
#include "ac_alfapro.h"
#include "ag_settings.h"

#include "at_rw_thread.h"

#define AT_THREAD_NAME "STREAMING_RW"

/********************************************
 * Local data
 */
static volatile int stop = 1;

static size_t media_buf_size;

//Interleaved socks - they are separate just for better undestanding - no simultanious use RT and IL socks!
static int read_il_sock = -1;
static int write_il_sock = -1;

static pthread_t rdwr_id;
static pthread_attr_t rdwr_attr;

static pthread_t hb_id;
static pthread_attr_t hb_attr;

static char* rdwr_buf;

static t_at_rtsp_session* the_session;

pu_queue_t* fromRW;

static volatile unsigned int bytes_passed = 0;
static volatile unsigned int bytes_accepted = 0;
static char err_buf[120];
static const char* stop_msg;

static volatile int hart_bit_off = 0;

static int hb_function() {
    int ret = 0;

    if(hart_bit_off) goto on_error;

    if(!the_session) goto on_error;

    if(!ac_alfaProOptions(the_session, 0)) goto on_error;

    ret = 1;
on_error:
    return ret;
}

static void* hart_beat(void* params) {
    pu_queue_t* q = (pu_queue_t*)params;

    lib_timer_clock_t rw_state = {0};
    lib_timer_init(&rw_state, 10);

    pu_log(LL_INFO, "%s start", __FUNCTION__);
    while(!stop) {
        sleep(1);
        if(lib_timer_alarm(rw_state)) {
            lib_timer_init(&rw_state, 10);
            if(!bytes_accepted) {
                pu_log(LL_ERROR, "%s: Cam stops provide the stream!", __FUNCTION__);
                pu_queue_push(q, stop_msg, strlen(stop_msg)+1);
                hart_bit_off = 1;
                sleep(3600);
            }
            else {
                bytes_accepted = 0;
            }

            if(!bytes_passed) {
                pu_log(LL_ERROR, "%s: WOWZA stops receive the stream!", __FUNCTION__);
                pu_queue_push(q, stop_msg, strlen(stop_msg)+1);
                hart_bit_off = 1;
                sleep(3600);
            }
            else {
                pu_log(LL_DEBUG, "%s: Bytes transferred = %d", __FUNCTION__, bytes_passed);
                bytes_passed = 0;
            }

        }
    }
    pu_log(LL_INFO, "%s stop", __FUNCTION__);
    sleep(1);
    pthread_exit(NULL);
}

static void thread_proc(const char* name, int read_sock, int write_sock, char* buf, pu_queue_t* q) {
    pu_log(LL_INFO, "%s start", name);
#ifdef RW_CYCLES
    int step = RW_CYCLES;
#endif
    bytes_passed = 0;
    bytes_accepted = 0;
    lib_timer_clock_t hart_beat = {0};
    lib_timer_init(&hart_beat, 30);     /* TODO: should be taken from session timeout parameter from RTSP session */
    while(!stop) {
        t_ac_udp_read_result ret;

        if(lib_timer_alarm(hart_beat)) {
            if(!hb_function()) {     /* hart beats from the Camera */
                pu_log(LL_ERROR, "%s: Camera is out: stop streaming process", __FUNCTION__);
                pu_queue_push(q, stop_msg, strlen(stop_msg)+1);
                sleep(3600);
            }
            else {
                lib_timer_init(&hart_beat, 30);
            }
        }


        ret = ac_udp_read(read_sock, buf, media_buf_size, 10);

        if(!ret.rc) {            /* timeout */
            continue;
        }
        else if(ret.rc < 0) {        /* Error */
            pu_log(LL_ERROR, "%s: Lost connection to the camera for %s", AT_THREAD_NAME, name);
            pu_queue_push(q, stop_msg, strlen(stop_msg) + 1);
            sleep(3600);
        }
        else
            bytes_accepted += ret.rc;

        int rt = 0;
        while(!rt && !stop) {
            rt = ac_udp_write(write_sock, buf, (size_t) ret.rc);
            if (rt < 0) {
                pu_log(LL_ERROR, "%s: Lost connection to the %s server", AT_THREAD_NAME, name);
                pu_queue_push(q, stop_msg, strlen(stop_msg) + 1);
                sleep(3600);
            }
        }

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
    pu_log(LL_INFO, "%s stop %s", AT_THREAD_NAME, name);
    sleep(1);
    pthread_exit(NULL);
}

static void *a_rdwr_the_thread(void* params) {
    thread_proc("INTERLEAVED_IO", read_il_sock, write_il_sock, rdwr_buf, fromRW);
    pthread_exit(NULL);
}

static int at_start_interleaved_rw_thread() {
    if(at_is_rw_thread_run()) return 1;

    stop_msg = ao_rw_error_answer(err_buf, sizeof(err_buf));

    stop = 0;
    if(pthread_attr_init(&hb_attr)) goto on_error;
    if(pthread_create(&hb_id, &hb_attr, &hart_beat, fromRW)) goto on_error;


    if(pthread_attr_init(&rdwr_attr)) goto on_error;
    if(pthread_create(&rdwr_id, &rdwr_attr, &a_rdwr_the_thread, NULL)) goto on_error;

    return 1;
on_error:
    stop = 1;
    return 0;
}

int at_set_interleaved_rw(int rd, int wr, t_at_rtsp_session* cam_sess) {
    if(at_is_rw_thread_run()) return 1;
    fromRW = aq_get_gueue(AQ_FromRW);

    read_il_sock = rd;
    write_il_sock = wr;

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

int at_start_rw() {
    hart_bit_off = 0;
    return at_start_interleaved_rw_thread();
}
/*****************************
 * Stop read streaming (join)
 */
void at_stop_rw() {
    if(!at_is_rw_thread_run()) {
        pu_log(LL_WARNING, "%s is already down", AT_THREAD_NAME);
        return;
    }
    void *ret;
    pthread_cancel(rdwr_id);
    pthread_cancel(hb_id);

    pthread_join(rdwr_id, &ret);
    pthread_join(hb_id, &ret);

    pthread_attr_destroy(&rdwr_attr);
    pthread_attr_destroy(&hb_attr);
//We do not close existing connections - they will be used for RTSP Teardown

    stop = 1;
    if(rdwr_buf) free(rdwr_buf);
    rdwr_buf = NULL;

    pu_log(LL_INFO, "%s: RW thread is down", AT_THREAD_NAME);
    pu_log(LL_INFO, "%s: HartBeat thread is down", AT_THREAD_NAME);
}
/*****************************
 * Check if read stream runs
 * @return 1 if runs 0 if not
 */
int at_is_rw_thread_run() {
 return !stop;
}
