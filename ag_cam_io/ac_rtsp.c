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
#include "lib_tcp.h"        /* for lib_tcp_local_ip */

#include "ag_defaults.h"
#include "ac_cam_types.h"
#include "ac_tcp.h"         /* for at_tcp_get_eth */
#include "ac_alfapro.h"
#include "ac_wowza.h"

#include "ac_rtsp.h"


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
            if(!ac_alfaProSetup(sess, AC_ALFA_VIDEO_SETUP)) return 0;
            return ac_alfaProSetup(sess, AC_ALFA_AUDIO_SETUP);
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

