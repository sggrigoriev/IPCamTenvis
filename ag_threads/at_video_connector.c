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
#include <ag_cam_io/ac_rtsp.h>

#include "pu_logger.h"
#include "pu_queue.h"

#include "ac_rtsp.h"
#include "ac_tcp.h"
#include "at_cam_video_read.h"
#include "at_cam_video_write.h"
#include "ag_settings.h"

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

static void say_cant_connect_to_vm();
static void say_connected_to_vm();
static void say_disconnected_to_vm();

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
    int video_sock;
    int cam_sock;
    int video_track_number;
    int tear_down_in_progress;
    char msg[LIB_HTTP_MAX_MSG_SIZE] = {0};
    char session_id[AC_CAM_RSTP_SESSION_ID_LEN];

    stop = 0;

    on_reconnect:
        video_sock = -1;
        cam_sock = -1;
        video_track_number = -1;
        tear_down_in_progress = 0;

        video_sock = video_server_connect();
        cam_sock = cam_connect();
        if((video_sock < 0) || (cam_sock < 0)) {
            say_cant_connect_to_vm();
            goto on_error;
        }
        say_connected_to_vm();
    while(!stop) {
        t_ac_rtsp_msg data;

        if(!ac_tcp_read(video_sock, msg, sizeof(msg))) goto on_error;
        data = rtsp_parse(msg);
        pu_log(LL_DEBUG, "%s: %s - came from videoserver", AT_THREAD_NAME, msg);

        switch(data.msg_type) {
            case AC_DESCRIBE:
                video_track_number = data.describe.video_track_number;
                break;
            case AC_ANNOUNCE:
                video_track_number = data.announce.video_track_number;
                break;
            case AC_SETUP:
                if (data.setup.track_number == video_track_number) {
                    ag_saveVideoServerPort(data.setup.client_port);
                    if(!at_start_video_write()) goto on_error;              /* Start wideo writer - 1/2 of streaming */
                }
                break;
            case AC_PLAY:
                strncpy(session_id, data.play.session_id, sizeof(session_id)-1);
                break;
            case AC_TEARDOWN:
                tear_down_in_progress = 1;
                break;
            case AC_UNDEFINED:
                pu_log(LL_ERROR, "%s: %s - Unrecognized message from video server", AT_THREAD_NAME, msg);
                break;
            default:
                break;
        }

        if(!ac_tcp_write(cam_sock, msg)) goto on_error;
        if(!ac_tcp_read(cam_sock, msg, sizeof(msg))) goto on_error;
        data = rtsp_parse(msg);
        pu_log(LL_DEBUG, "%s: %s - came from camers", AT_THREAD_NAME, msg);

        switch(data.msg_type) {
            case AC_SETUP:
                if (data.setup.track_number == video_track_number) {
                    ag_saveCamPort(data.setup.server_port);
                    if(!at_start_video_read()) goto on_error;               /* Start wideo reader - 2/2 of streaming */
                }
                break;
            case AC_JUST_ANSWER:
                if (tear_down_in_progress) {
                    stop = 1;
                    say_disconnected_to_vm();
                }
                break;
            default:
                break;
        }
    }
    on_error:
        stop_streaming();
        ac_rtsp_disconnect();
        if(!tear_down_in_progress) {
            pu_log(LL_ERROR, "%s: Reconnect due to connection problems", AT_THREAD_NAME);
            goto on_reconnect;
        }
    pthread_exit(NULL);
}

static void stop_streaming() {
    at_set_stop_video_read();
    at_set_stop_video_write();

    if(at_is_video_read_run()) at_stop_video_read();
    if(at_is_video_write_run()) at_stop_video_write();
}
