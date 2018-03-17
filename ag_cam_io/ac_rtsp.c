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

#include <memory.h>
#include <assert.h>

#include "pu_logger.h"

#include "ag_defaults.h"
#include "ac_cam_types.h"
#include "ac_alfapro.h"
#include "ac_wowza.h"
#include "at_cam_video_write.h"
#include "at_cam_video_read.h"

#include "ac_rtsp.h"
#include "ac_udp.h"
#include "ac_tcp.h"


/*******************************************************************************************/

t_at_rtsp_session* ac_rtsp_init(t_ac_rtsp_device device, const char* url, const char* session_id) {
    assert(url); assert(session_id);
    t_at_rtsp_session* sess = calloc(sizeof(t_at_rtsp_session), 1);
    if(!sess) {
        pu_log(LL_ERROR, "%s Memory allocation error at %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    sess->device = device;
    sess->state = AC_STATE_UNDEF;
    if(sess->url = strdup(url), !sess->url) {
        pu_log(LL_ERROR, "%s Memory allocation error at %d", __FUNCTION__, __LINE__);
        goto on_error;
    }

    int rc;

    switch (device) {
        case AC_CAMERA: {
            rc = ac_alfaProInit(sess);
            if (sess->video_pair.dst.ip = strdup("0.0.0.0"), !sess->video_pair.dst.ip) {
                pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
                goto on_error;
            }
            if (sess->audio_pair.dst.ip = strdup("0.0.0.0"), !sess->audio_pair.dst.ip) {
                pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
                goto on_error;
            }
            sess->video_pair.dst.port.rtp = AC_FAKE_CLIENT_READ_PORT;
            sess->video_pair.dst.port.rtcp = AC_FAKE_CLIENT_READ_PORT+1;
            sess->audio_pair.dst.port.rtp = AC_FAKE_CLIENT_READ_PORT+2;
            sess->audio_pair.dst.port.rtcp = AC_FAKE_CLIENT_READ_PORT+3;
        }
            break;
        case AC_WOWZA:
            sess->video_pair.src.port.rtp = AC_FAKE_CLIENT_WRITE_PORT;
            sess->video_pair.src.port.rtcp = AC_FAKE_CLIENT_WRITE_PORT+1;
            sess->audio_pair.src.port.rtp = AC_FAKE_CLIENT_WRITE_PORT+2;
            sess->audio_pair.src.port.rtcp = AC_FAKE_CLIENT_WRITE_PORT+3;
            rc = ac_WowzaInit(sess, session_id);
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            rc = 0;
            break;
    }
    if(rc) {
        return sess;
    }

on_error:
    if(sess) {
        if(sess->url) free(sess->url);
        if(sess->video_pair.dst.ip) free(sess->video_pair.dst.ip);
        if(sess->audio_pair.dst.ip) free(sess->audio_pair.dst.ip);
        free(sess);
    }
    return NULL;
}
void ac_rtsp_down(t_at_rtsp_session* sess) {
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
    if(sess->url) free(sess->url);
    if(sess->video_pair.src.ip) free(sess->video_pair.src.ip);
    if(sess->video_pair.dst.ip) free(sess->video_pair.dst.ip);

    if(sess->audio_pair.src.ip) free(sess->audio_pair.src.ip);
    if(sess->audio_pair.dst.ip) free(sess->audio_pair.dst.ip);

    if(sess->rtsp_session_id) free(sess->rtsp_session_id);
    free(sess);
}

int ac_req_options(t_at_rtsp_session* sess) {
    assert(sess);

    switch(sess->device) {
        case AC_CAMERA:
            return ac_alfaProOptions(sess);
         case AC_WOWZA:
            return ac_WowzaOptions(sess);
         default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            return 0;
    }
    return 0;
}
int ac_req_cam_describe(t_at_rtsp_session* sess, char** dev_description) {
    assert(sess);
    *dev_description = NULL;

    if(sess->device != AC_CAMERA) {
        pu_log(LL_ERROR, "%s: Wrong device type %d. Expected %d", __FUNCTION__, sess->device, AC_CAMERA);
        return 0;
    }

    char body[1000];
    if(!ac_alfaProDescribe(sess, body, sizeof(body))) return 0;

    if(*dev_description = strdup(body), !dev_description) {
        pu_log(LL_ERROR, "%s: Mempry allocation error", __FUNCTION__);
        return 0;
    }

    return 1;
}
int ac_req_vs_announce(t_at_rtsp_session* sess, const char* dev_description) {
    assert(sess); assert(dev_description);

    return ac_WowzaAnnounce(sess, dev_description);
}
int ac_req_setup(t_at_rtsp_session* sess) {
    assert(sess);

    switch(sess->device) {
        case AC_CAMERA:
            return ac_alfaProSetup(sess, AC_ALFA_VIDEO_SETUP);
        case AC_WOWZA:
            return ac_WowzaSetup(sess);
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            return 0;
    }
    return 0;
}
int ac_req_play(t_at_rtsp_session* sess) {
    assert(sess);

    switch(sess->device) {
        case AC_CAMERA:
            return ac_alfaProPlay(sess);
        case AC_WOWZA:
            return ac_WowzaPlay(sess);
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            return 0;
    }
    return 0;
}
int ac_req_teardown(t_at_rtsp_session* sess) {
    assert(sess);

    switch(sess->device) {
        case AC_CAMERA:
            return ac_alfaProTeardown(sess);
        case AC_WOWZA:
            return ac_WowzaTeardown(sess);
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            return 0;
    }
    return 0;
}

int ac_start_rtsp_streaming(t_rtsp_pair in, t_rtsp_pair out) {
    if(!at_start_video_read(in)) goto on_error;
    if(!at_start_video_write(out)) goto on_error;

    return 1;
on_error:
    ac_close_connection(in.rtp);
    ac_close_connection(in.rtcp);
    ac_close_connection(out.rtp);
    ac_close_connection(out.rtcp);

    return 0;
}
void ac_stop_rtsp_streaming() {
    at_stop_video_write();
    at_stop_video_read();
}

static const uint8_t RTP_INIT_REQ[] = {0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t RTCP_INIT_REQ[] = {0x80, 0xc9, 0, 01, 0, 0, 0, 0};

int ac_open_connecion(t_ac_rtsp_pair_ipport video_in, t_ac_rtsp_pair_ipport video_out, t_rtsp_pair* in, t_rtsp_pair* out) {
    in->rtp = -1; in->rtcp = -1; out->rtp = -1, out->rtcp = -1;
//Camera
    if((in->rtp = ac_udp_p2p_connection(video_in.src.ip, video_in.src.port.rtp, video_in.dst.port.rtp)) < 0) {
        pu_log(LL_ERROR, "%s Can't open UDP socket for Camera RTP stream. Bye.", __FUNCTION__);
        goto on_error;
    }
    if((in->rtcp = ac_udp_p2p_connection(video_in.src.ip, video_in.src.port.rtcp, video_in.dst.port.rtcp)) < 0) {
        pu_log(LL_ERROR, "%s Can't open UDP socket for Camera RTCP stream. Bye.", __FUNCTION__);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: Cam-Agent connection RTP: %s:%d-%d; RTCP connection %s:%d-%d", __FUNCTION__,video_in.src.ip, video_in.src.port.rtp, video_in.dst.port.rtp, video_in.src.ip, video_in.src.port.rtcp, video_in.dst.port.rtcp);

//Player - UDP connection
    if((out->rtp = ac_udp_p2p_connection(video_out.dst.ip, video_out.dst.port.rtp, video_out.src.port.rtp)) < 0) {
        pu_log(LL_ERROR, "%s Can't open UDP socket for Camera RTP stream. Bye.", __FUNCTION__);
        goto on_error;
    }
    if((out->rtcp = ac_udp_p2p_connection(video_out.dst.ip, video_out.dst.port.rtcp, video_out.src.port.rtcp)) < 0) {
        pu_log(LL_ERROR, "%s Can't open UDP socket for Camera RTCP stream. Bye.", __FUNCTION__);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: Player-Agent connection RTP: %s:%d-%d; RTCP connection %s:%d-%d", __FUNCTION__,video_out.dst.ip, video_out.dst.port.rtp, video_out.src.port.rtp, video_out.dst.ip, video_out.dst.port.rtcp, video_out.src.port.rtcp);

/*
//Player - TCP connection
    if((out->rtp = ac_tcp_client_connect(video_out.dst.ip, video_out.dst.port.rtp)) < 0) {
        pu_log(LL_ERROR, "%s Can't open UDP socket for Camera RTP stream. Bye.", __FUNCTION__);
        goto on_error;
    }
    if((out->rtcp = ac_tcp_client_connect(video_out.dst.ip, video_out.dst.port.rtcp)) < 0) {
        pu_log(LL_ERROR, "%s Can't open UDP socket for Camera RTCP stream. Bye.", __FUNCTION__);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: Player-Agent connection RTP: %s:%d-%d; RTCP connection %s:%d-%d", __FUNCTION__,video_out.dst.ip, video_out.dst.port.rtp, video_out.src.port.rtp, video_out.dst.ip, video_out.dst.port.rtcp, video_out.src.port.rtcp);
*/
/*
//Initial RTP/RTCP request to Cam
    if (!ac_udp_write(in->rtp, RTP_INIT_REQ, sizeof(RTP_INIT_REQ))) {
        pu_log(LL_ERROR, "%s: RTP setup failed", __FUNCTION__);
        goto on_error;
    }
    if (!ac_udp_write(in->rtcp, RTCP_INIT_REQ, sizeof(RTCP_INIT_REQ))) {
        pu_log(LL_ERROR, "%s: RTCP setup failed", __FUNCTION__);
        goto on_error;
    }
*/
    return 1;
on_error:
    if(in->rtp >= 0) ac_close_connection(in->rtp);
    if(in->rtcp >= 0) ac_close_connection(in->rtcp);
    if(out->rtp >= 0) ac_close_connection(out->rtp);
    if(out->rtcp >= 0) ac_close_connection(out->rtcp);
    in->rtp = -1; in->rtcp = -1; out->rtp = -1, out->rtcp = -1;
    return 0;
}



