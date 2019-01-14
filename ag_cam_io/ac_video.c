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
 Created by gsg on 25/02/18.
*/

#include <string.h>
#include <pthread.h>

#include "pu_logger.h"

#include "ao_cmd_data.h"
#include "ao_cmd_cloud.h"
#include "au_string.h"
#include "ao_cmd_data.h"
#include "ag_settings.h"

#include "at_ws.h"

#include "ac_cloud.h"
#include "ac_cam.h"
#include "ac_cam_types.h"

#include "ac_alfapro.h"
#include "ac_rtsp.h"
#include "ac_wowza.h"

#include "ac_video.h"

/* Local variables */

static t_ao_conn video_conn = {{0},0,{0}};

/*
 * Get video params params from cloud: 3 steps f
 * rom https://presence.atlassian.net/wiki/spaces/EM/pages/164823041/Setup+IP+Camera+connection
*/
static int get_vs_conn_params(t_ao_conn* video, t_ao_conn* ws) {
    if(!ac_cloud_get_params(video->url, sizeof(video->url), &video->port, video->auth, sizeof(video->auth), ws->url, sizeof(ws->url), &ws->port, ws->auth, sizeof(ws->auth))) {
        return 0;
    }
    pu_log(LL_DEBUG, "%s: Video Connection parameters: URL = %s, PORT = %d, SessionId = %s", __FUNCTION__, video->url, video->port, video->auth);
    pu_log(LL_DEBUG, "%s: WS Connection parameters: URL = %s, PORT = %d, SessionId = %s", __FUNCTION__, ws->url, ws->port, ws->auth);
    return 1;
}

/*
 * Run RTSP exchange and video streaming at the end (and audio - later)
 */

static int ac_send_stream_initiation() {
    char buf[512];
    return at_ws_send(ao_cmd_cloud_connection_request(buf, sizeof(buf), video_conn.auth));
}
static int ac_send_stream_confirmation(){
    char buf[128];
    return at_ws_send(ao_cmd_cloud_stream_approve(buf, sizeof(buf), video_conn.auth));
}

t_at_rtsp_session* CAM_SESSION;
t_at_rtsp_session* PLAYER_SESSION;

static void shutdown_proc() {
    if(PLAYER_SESSION) ac_rtsp_deinit(PLAYER_SESSION);
    if(CAM_SESSION) ac_rtsp_deinit(CAM_SESSION);

    CAM_SESSION = NULL;
    PLAYER_SESSION = NULL;
}
static int init_proc() {
    CAM_SESSION = NULL;
    PLAYER_SESSION = NULL;

    if(CAM_SESSION = ac_rtsp_init(AC_CAMERA, NULL, -1, NULL), !CAM_SESSION) goto on_error;
    pu_log(LL_DEBUG, "%s (AC_CAMERA) done", __FUNCTION__);

    if(PLAYER_SESSION = ac_rtsp_init(AC_WOWZA, video_conn.url,  video_conn.port, video_conn.auth), !PLAYER_SESSION) goto on_error;
    pu_log(LL_DEBUG, "%s (AC_WOWZA) done", __FUNCTION__);

    return 1;
    on_error:
    shutdown_proc();
    return 0;
}

static int process_connect() {
    if(!ac_req_options(CAM_SESSION)) return 0;
    if(!ac_req_options(PLAYER_SESSION)) return 0;

    return 1;
}
static int process_describe() {
    char sdp[1000] = {0};
    int rc = 0;

    if(!ac_req_cam_describe(CAM_SESSION, sdp, sizeof(sdp))) goto on_exit;
    if(!ac_req_vs_announce(PLAYER_SESSION, sdp)) goto on_exit;

    rc = 1;
    on_exit:
    return rc;
}
static int process_setup(int is_video, int is_audio) {
    if(!ac_req_setup(CAM_SESSION, is_video, is_audio)) return 0;
    if(!ac_req_setup(PLAYER_SESSION, is_video, is_audio)) return 0;

    return 1;
}
static int process_play() {
    if(!ac_req_play(CAM_SESSION)) return 0;
    if(!ac_req_play(PLAYER_SESSION)) return 0;

    return 1;
}

/*
* 1. Get streaming & WS connection parameters
* 2. Run WebSocket interface waiting for streaming order
* 3. Send streaming initiation request
*/
int ac_connect_video() {
    t_ao_conn ws_conn = {{0},0,{0}};

    if(!get_vs_conn_params(&video_conn, &ws_conn)) {
        pu_log(LL_ERROR, "%s: error video stream parameters retrieve", __FUNCTION__);
        return 0;
    }
    if(!at_ws_start(ws_conn.url, ws_conn.port, "/streaming/camera", ws_conn.auth)) {
        pu_log(LL_ERROR, "%s: Error start WEB socket connector, exit.", __FUNCTION__);
        return 0;
    }
    if(!ac_send_stream_initiation()) {
        pu_log(LL_ERROR, "%s: Error streaming initiation, exit.", __FUNCTION__);
        sleep(1);
        at_ws_stop();
        return 0;
    }
    return 1;
}
void ac_disconnect_video() {
    at_ws_stop();
    video_conn.port = -1;
    video_conn.url[0] = '\0';
    video_conn.auth[0] = '\0';
}

/*
 *  1. Perform RTSP negotiations for CAM & Wowza
 *  2. Run streaming
 *  3. Send to WS Stream confirmation request
*/
static int video_on = 0;
int ac_start_video(int is_video, int is_audio) {
    if(video_on) {
        pu_log(LL_WARNING, "%s: Video already on! Suspicious call", __FUNCTION__);
        return 1;
    }
    if(!init_proc()) {
        pu_log(LL_ERROR, "%s: Video initiation error, exit", __FUNCTION__);
        goto on_error;
    }
    if(!process_connect()) {
        pu_log(LL_ERROR, "%s: process connect error", __FUNCTION__);
        goto on_error;
    }
    if(!process_describe()) {
        pu_log(LL_ERROR, "%s: process describe error", __FUNCTION__);
        goto on_error;
    }
    if(!process_setup(is_video, is_audio)) {
        pu_log(LL_ERROR, "%s: process setup error", __FUNCTION__);
        goto on_error;
    }

    if(!ac_rtsp_open_streaming_connecion(CAM_SESSION, PLAYER_SESSION)) {    //Open streaming sockets
        pu_log(LL_ERROR, "%s: open streaming connections error", __FUNCTION__);
        goto on_error;
    }

    if(!process_play()) {
        pu_log(LL_ERROR, "%s: process play error", __FUNCTION__);
        goto on_error;
    }

    if(!ac_rtsp_start_streaming()) {                                        //Start streaming thread
        pu_log(LL_ERROR, "%s: start streaming error", __FUNCTION__);
        goto on_error;
    }

    if(!ac_send_stream_confirmation()) {                                            //Send to WS streaming start confirmation
        pu_log(LL_ERROR, "%s: Stream confirmation send to WS failed", __FUNCTION__);
        goto on_error;
    }
    video_on = 1;
    return 1;
    on_error:
    ac_rtsp_stop_streaming();
    ac_rtsp_close_streaming_connecion();
    shutdown_proc();
    return 0;
}
void ac_stop_video() {
    if(!video_on) {
        pu_log(LL_WARNING, "%s: Video is already off. Suspicious call. Ignored", __FUNCTION__);
        return;
    }
    ac_rtsp_stop_streaming();
    ac_rtsp_close_streaming_connecion();

    ac_req_teardown(CAM_SESSION);
    ac_req_teardown(PLAYER_SESSION);

    shutdown_proc();
    video_on = 0;
}

/*****************************************
 * To WebSocket messages
 */

static pthread_mutex_t local_mutex = PTHREAD_MUTEX_INITIALIZER;
static char stream_error[256]={0};

void ac_set_stream_error(const char* err) {
    pthread_mutex_lock(&local_mutex);
    strncpy(stream_error, err, sizeof(stream_error)-1);
    stream_error[sizeof(stream_error)-1] = '\0';
    pthread_mutex_unlock(&local_mutex);
}
void ac_clear_stream_error() {
    pthread_mutex_lock(&local_mutex);
    stream_error[0] = '\0';
    pthread_mutex_unlock(&local_mutex);
}
const char* ac_get_stream_error(char* buf, size_t size) {
    pthread_mutex_lock(&local_mutex);
    strncpy(buf, stream_error, size-1);
    buf[size-1] = '\0';
    pthread_mutex_unlock(&local_mutex);
    return buf;
}
/* return 1 if stlen() > 0 */
int ac_is_stream_error() {
    int ret;
    pthread_mutex_lock(&local_mutex);
    ret = strlen(stream_error) > 0;
    pthread_mutex_unlock(&local_mutex);
    return ret;
}

const char* ac_get_session_id(char* buf, size_t size) {
    strncpy(buf, video_conn.auth, size-1);
    buf[size-1] = '\0';
    return buf;
}
