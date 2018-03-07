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

#include <ag_converter/ao_cmd_data.h>
#include <ag_converter/ao_cmd_cloud.h>
#include "pu_logger.h"

#include "au_string.h"
#include "ao_cmd_data.h"
#include "ag_settings.h"

#include"ab_ring_bufer.h"

#include "at_cam_video_read.h"
#include "at_cam_video_write.h"
#include "at_ws.h"

#include "ac_cloud.h"
#include "ac_cam.h"
#include "ac_cam_types.h"

#include "ac_alfapro.h"
#include "ac_rtsp.h"
#include "ac_wowza.h"

#include "ac_video.h"

/* Local variables */

static t_ao_conn video_conn = {0};

/* Get video params params from cloud: 3 steps from https://presence.atlassian.net/wiki/spaces/EM/pages/164823041/Setup+IP+Camera+connection */
static int get_vs_conn_params(t_ao_conn* video, t_ao_conn* ws) {
    if(!ac_cloud_get_params(video->url, sizeof(video->url), &video->port, video->auth, sizeof(video->auth), ws->url, sizeof(ws->url), &ws->port, ws->auth, sizeof(ws->auth))) {
        return 0;
    }
    pu_log(LL_DEBUG, "%s: Video Connection parameters: URL = %s, PORT = %d, SessionId = %s", __FUNCTION__, video->url, video->port, video->auth);
    pu_log(LL_DEBUG, "%s: WS Connection parameters: URL = %s, PORT = %d, SessionId = %s", __FUNCTION__, ws->url, ws->port, ws->auth);
    return 1;
}

/*
* 1. Get streaming & WS connection parameters
* 2. Make initial Cam setuo
* 3. Run WebSocket interface
* 4. Run Cam's async interface (phase - II)
*/
int ac_connect_video() {

    t_ao_conn ws_conn = {0};

    if(!get_vs_conn_params(&video_conn, &ws_conn)) {
        pu_log(LL_ERROR, "%s: error video stream parameters retrieve", __FUNCTION__);
        return 0;
    }

    if(!ac_cam_init()) {
        pu_log(LL_ERROR, "%s: error camera initiation", __FUNCTION__);
        return 0;
    }

    if(!at_ws_start(ws_conn.url, ws_conn.port, "/streaming/camera", ws_conn.auth)) {
        pu_log(LL_ERROR, "%s: Error start WEB socket connector, exit.", __FUNCTION__);
        return 0;
    }

    char buf[128];

    if(!at_ws_send(ao_stream_request(buf, sizeof(buf), ws_conn.auth))) {
        pu_log(LL_ERROR, "%s: Error sending stream request to WS, exit.", __FUNCTION__);
        return 0;
    }

    return 1;
}

void ac_disconnect_video() {
    at_ws_stop();
}

/*
 * Run RTSP exchange and video streaming at the end (and audio - later)
 */

t_at_rtsp_session* CAM_SESSION;
t_at_rtsp_session* PLAYER_SESSION;

static void shutdown_proc() {
    if(PLAYER_SESSION) ac_rtsp_down(PLAYER_SESSION);
    if(CAM_SESSION) ac_rtsp_down(CAM_SESSION);
    ab_close();         /* Erase videostream buffer */

    CAM_SESSION = NULL;
    PLAYER_SESSION = NULL;
    video_conn.port = -1;
    video_conn.url[0] = '\0';
    video_conn.auth[0] = '\0';
}
static int init_proc() {
    CAM_SESSION = NULL;
    PLAYER_SESSION = NULL;
    video_conn.port = -1;
    video_conn.url[0] = '\0';
    video_conn.auth[0] = '\0';

    /* Setup the ring buffer for video streaming */
    if(!ab_init(0, ag_getVideoChunksAmount())) {
        pu_log(LL_ERROR, "%s: Videostreaming buffer allocation error", __FUNCTION__);
        return 0;
    }
    char url[LIB_HTTP_MAX_URL_SIZE];
    ac_makeAlfaProURL(url, sizeof(url), ag_getCamIP(), ag_getCamPort(), ag_getCamLogin(), ag_getCamPassword(), ag_getCamResolution());

    if(CAM_SESSION = ac_rtsp_init(AC_CAMERA, url, ""), !CAM_SESSION) goto on_error;

    ac_make_wowza_url(url, sizeof(url), "rtsp", video_conn.url, video_conn.port, video_conn.auth);
    if(PLAYER_SESSION = ac_rtsp_init(AC_WOWZA, url,  video_conn.auth), !PLAYER_SESSION) goto on_error;

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
    char *description = NULL;
    int rc = 0;

    if(!ac_req_cam_describe(CAM_SESSION, &description)) goto on_exit;
    if(!ac_req_vs_announce(PLAYER_SESSION, description)) goto on_exit;

    rc = 1;
on_exit:
    if(description) free(description);
    return rc;
}
static int process_setup(t_rtsp_pair* cam_io, t_rtsp_pair* player_io) {    /* NB! Video stream only!*/

    if(!ac_req_setup(CAM_SESSION)) return 0;
    if(!ac_req_setup(PLAYER_SESSION)) return 0;

    if(!ac_open_connecion(CAM_SESSION->video_pair, PLAYER_SESSION->video_pair, cam_io, player_io)) return 0;

    return 1;
}
static int process_play(t_rtsp_pair cam_io, t_rtsp_pair player_io) {

    if(!ac_req_play(CAM_SESSION)) return 0;
    if(!ac_req_play(PLAYER_SESSION)) return 0;

    if(!ac_start_rtsp_streaming(cam_io, player_io)) return 0;

    return 1;
}

int ac_start_video() {
    t_rtsp_pair cam_io = {-1,-1};
    t_rtsp_pair player_io = {-1,-1};

    if(!init_proc()) {
        pu_log(LL_ERROR, "%s: initiation error, exit", __FUNCTION__);
        return 0;
    }

    if(!process_connect()) {
        pu_log(LL_ERROR, "%s: process connect error", __FUNCTION__);
        goto on_error;
    }
    if(!process_describe()) {
        pu_log(LL_ERROR, "%s: process describe error", __FUNCTION__);
        goto on_error;
    }
    if(!process_setup(&cam_io, &player_io)) {
        pu_log(LL_ERROR, "%s: process setup error", __FUNCTION__);
        goto on_error;
    }
    if(!process_play(cam_io, player_io)) {
        pu_log(LL_ERROR, "%s: process play error", __FUNCTION__);
        goto on_error;
    }

    char buf[128];

    if(!at_ws_send(ao_stream_approve(buf, sizeof(buf), video_conn.auth))) {
        pu_log(LL_ERROR, "%s - %s: Error sending stream approve to WS, exit.", __FUNCTION__);
        return 0;
    }


    return 1;
on_error:
    shutdown_proc();
    return 0;
}

/*
 * Stops videostreaming
*/
void ac_stop_video() {
    ac_req_teardown(CAM_SESSION);
    ac_req_teardown(PLAYER_SESSION);

    ac_stop_rtsp_streaming();
}
/*
 * Return 1 if treaming threads work
 */
int ac_streaming_run() {
    return (at_is_video_read_run() && at_is_video_write_run());
}

void ac_send_stream_confirmation() {
    char buf[512];
    at_ws_send(ao_connection_request(buf, sizeof(buf), video_conn.auth));
}

