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
 Created by gsg on 13/11/17.
*/

#include <assert.h>
#include <memory.h>
#include <lib_http.h>

#include "pu_logger.h"

#include "au_string.h"

#include "ag_settings.h"
#include "ag_defaults.h"
#include "ac_udp.h"

#include "ac_wowza.h"
#include "ac_alfapro.h"
#include "at_rw_thread.h"

//#include "at_cam_video_write.h"
//#include "at_cam_video_read.h"
//#include "ac_tcp.h"

#include "ac_rtsp.h"
#include "ac_cam_types.h"
#include "ac_tcp.h"

/*******************************************************************************************/

static void close_il_connection() {
/*    int in, out;
    at_get_interleaved_rw(&in, &out);
     if (in >= 0) ac_udp_close_connection(in); */
//NB! out is under GST responcibility!
}
static int open_il_connection(t_at_rtsp_session* sess_in, t_at_rtsp_session* sess_out) {
    int rd_sock = -1, wr_sock = -1;

    if(rd_sock = getAlfaProConnSocket(sess_in), rd_sock < 0) {
        pu_log(LL_ERROR, "%s: error retrieving Cam's read socket for interleaved mode", __FUNCTION__);
        return 0;
    }
    if(wr_sock = getWowzaConnSocket(sess_out), rd_sock < 0) {
        pu_log(LL_ERROR, "%s: error retrieving Wowza's write socket for interleaved mode", __FUNCTION__);
        return 0;
    }
// Using existing connections from RTSP negotiations - do not need opening TCP connections
    if(!at_set_interleaved_rw(rd_sock, wr_sock, sess_in)) {
        pu_log(LL_ERROR, "%s: Can't initiate streaming treads for interleaved mode", __FUNCTION__);
        return 0;
    }
    pu_log(LL_DEBUG, "%s: streaming thread for interleaved mode are initiated. Cam socket = %d, Wowza socket = %d", __FUNCTION__, rd_sock, wr_sock);
    return 1;
}

t_at_rtsp_session* ac_rtsp_init(t_ac_rtsp_device device, const char* ip, int port, const char* session_id) {
    t_at_rtsp_session* sess = calloc(sizeof(t_at_rtsp_session), 1);
    if(!sess) {
        pu_log(LL_ERROR, "%s Memory allocation error at %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    sess->device = device;
    sess->state = AC_STATE_UNDEF;
    sess->audio_url = NULL;
    sess->video_url = NULL;

    if(ag_isCamInterleavedMode()) {
        sess->media.il_media.port = (sess->device == AC_WOWZA)?port:ag_getCamPort();
        if(sess->media.il_media.ip = (sess->device == AC_WOWZA)?au_strdup(ip):au_strdup(ag_getCamIP()), !sess->media.il_media.ip) {
            pu_log(LL_ERROR, "%s: memory alloation error at %d", __FUNCTION__, __LINE__);
            goto on_error;
        }
    }

    int rc;
    char url[LIB_HTTP_MAX_URL_SIZE];

    switch (device) {
        case AC_CAMERA: {
            /* rtsp://<ip>:port/0/av1 */
            snprintf(url, sizeof(url)-1, "rtsp://%s:%d/%s/%s%s", ag_getCamIP(), ag_getCamPort(), ag_getCamPostfix(), ag_getCamMode(), ag_getCamChannel());
            if(sess->url = au_strdup(url), !sess->url) {
                pu_log(LL_ERROR, "%s: Memory allocation error ar %d", __FUNCTION__, __LINE__);
                goto on_error;
            }

            if(rc = ac_alfaProInit(sess), !rc) goto on_error;
        }
            break;
        case AC_WOWZA:
            pu_log(LL_DEBUG, "%s Wowza section start. ip=%s, port=%d, session_id=%s", __FUNCTION__, ip, port, session_id);
            ac_make_wowza_url(url, sizeof(url), "rtsp", ip, port, session_id);
            if(sess->url = au_strdup(url), !sess->url) {
                pu_log(LL_ERROR, "%s: Memory allocation error ar %d", __FUNCTION__, __LINE__);
                goto on_error;
            }
            pu_log(LL_DEBUG, "%s before ac_WowzaInit", __FUNCTION__);
            if(rc = ac_WowzaInit(sess, session_id), !rc) goto on_error;
            pu_log(LL_DEBUG, "%s after ac_WowzaInit", __FUNCTION__);
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            goto on_error;
    }

    if(!ag_isCamInterleavedMode()) {
        if (sess->media.rt_media.video.dst.ip = au_strdup("0.0.0.0"), !sess->media.rt_media.video.dst.ip) {
            pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
            goto on_error;
        }
        if (sess->media.rt_media.audio.dst.ip = au_strdup("0.0.0.0"), !sess->media.rt_media.audio.dst.ip) {
            pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
            goto on_error;
        }
        sess->media.rt_media.video.dst.port.rtp = AC_FAKE_CLIENT_READ_PORT;
        sess->media.rt_media.video.dst.port.rtcp = AC_FAKE_CLIENT_READ_PORT + 1;
        sess->media.rt_media.audio.dst.port.rtp = AC_FAKE_CLIENT_READ_PORT + 2;
        sess->media.rt_media.audio.dst.port.rtcp = AC_FAKE_CLIENT_READ_PORT + 3;
    }
    if(rc) {
        return sess;
    }

    on_error:
    pu_log(LL_DEBUG, "%s error section start", __FUNCTION__);
    if(sess) ac_rtsp_deinit(sess);
    return NULL;
}
void ac_rtsp_deinit(t_at_rtsp_session* sess) {
    assert(sess);
    switch(sess->device) {
        case AC_CAMERA:
            ac_alfaProDown(sess);
            break;
        case AC_WOWZA:
            ac_WowzaDown(sess);
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            break;
    }
    if (sess->url) free(sess->url);
    if (sess->rtsp_session_id) free(sess->rtsp_session_id);
    if (sess->video_url) free(sess->video_url);
    if (sess->audio_url) free(sess->audio_url);

    if(!ag_isCamInterleavedMode()) {
        if (sess->media.rt_media.video.src.ip) free(sess->media.rt_media.video.src.ip);
        if (sess->media.rt_media.video.dst.ip) free(sess->media.rt_media.video.dst.ip);

        if (sess->media.rt_media.audio.src.ip) free(sess->media.rt_media.audio.src.ip);
        if (sess->media.rt_media.audio.dst.ip) free(sess->media.rt_media.audio.dst.ip);
    }
    else {
        if (sess->media.il_media.ip) free(sess->media.il_media.ip);
    }
    free(sess);
}

int ac_req_options(t_at_rtsp_session* sess) {
    assert(sess);

    switch(sess->device) {
        case AC_CAMERA:
            return ac_alfaProOptions(sess, 0);
        case AC_WOWZA:
            return ac_WowzaOptions(sess);
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            return 0;
    }
    return 0;
}
int ac_req_cam_describe(t_at_rtsp_session* sess, char* dev_description, size_t size) {
    assert(sess);

    if(sess->device != AC_CAMERA) {
        pu_log(LL_ERROR, "%s: Wrong device type %d. Expected %d", __FUNCTION__, sess->device, AC_CAMERA);
        return 0;
    }

    if(!ac_alfaProDescribe(sess, dev_description, size)) return 0;

    return 1;
}
int ac_req_vs_announce(t_at_rtsp_session* sess, const char* dev_description) {
    assert(sess); assert(dev_description);
/* NB! dv_description modifications done inside of ac_WowzaAnnounce */
/* video & audio urls are made there as well */
    return ac_WowzaAnnounce(sess, dev_description);
}
int ac_req_setup(t_at_rtsp_session* sess, int is_video, int is_audio) {
    assert(sess);
    int ret, ret_v;

    switch(sess->device) {
        case AC_CAMERA:
            ret_v = (is_video)?ac_alfaProSetup(sess, AC_RTSP_VIDEO_SETUP):1;
            ret = (ret_v && is_audio)?ac_alfaProSetup(sess, AC_RTSP_AUDIO_SETUP):ret_v;
            break;
        case AC_WOWZA:
            ret_v = (is_video)?ac_WowzaSetup(sess, AC_RTSP_VIDEO_SETUP):1;
            ret = (ret_v && is_audio)?ac_WowzaSetup(sess, AC_RTSP_AUDIO_SETUP):ret_v;
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            ret = 0;
            break;
    }
    return ret;
}
int ac_req_play(t_at_rtsp_session* sess) {
    assert(sess);
    int ret;
    switch(sess->device) {
        case AC_CAMERA:
            ret = ac_alfaProPlay(sess);
            break;
        case AC_WOWZA:
            ret = ac_WowzaPlay(sess);
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            ret = 0;
            break;
    }
    return ret;
}
int ac_req_teardown(t_at_rtsp_session* sess) {
    assert(sess);
    int ret;
    switch(sess->device) {
        case AC_CAMERA:
            ret = ac_alfaProTeardown(sess);
            break;
        case AC_WOWZA:
            ret = ac_WowzaTeardown(sess);
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            ret = 0;
            break;
    }
    return ret;
}

int ac_rtsp_open_streaming_connecion(t_at_rtsp_session* sess_in, t_at_rtsp_session* sess_out) {
    return open_il_connection(sess_in, sess_out);
}
void ac_rtsp_close_streaming_connecion() {
    close_il_connection();
}

int ac_rtsp_start_streaming() {
    return at_start_rw();
}
void ac_rtsp_stop_streaming() {
    at_stop_rw();
}
