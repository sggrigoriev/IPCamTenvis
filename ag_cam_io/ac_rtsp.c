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
#include "ac_alfapro.h"
#include "ac_wowza.h"

#include "ac_rtsp.h"

static char* make_url(const char* url, const char* session_id) {
    size_t len = strlen(url) + strlen(session_id) + strlen(AC_RTSP_HEAD)+1;
    char* ret = calloc(len, 1);
    if(!ret) {
        pu_log(LL_ERROR, "%s: memory allocation error!", __FUNCTION__);
        return NULL;
    }
    strcpy(ret, AC_RTSP_HEAD);
    strcat(ret, url);
    strcat(ret, session_id);
    return ret;
}

/*******************************************************************************************/

t_at_rtsp_session* ac_rtsp_init(t_ac_rtsp_device device) {
    switch (device) {
        case AC_CAMERA:
            return ac_alfaProInit();
        case AC_WOWZA:
            return ac_WowzaInit();
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, device);
            return NULL;
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
}

int ac_open_session(t_at_rtsp_session* sess, const char* url, const char* session_id) {
    assert(sess); assert(url); assert(session_id);

    if(sess->url = make_url(url, session_id), !sess->url) goto on_error;

    switch(sess->device) {
        case AC_CAMERA:
            if(!ac_alfaProOpenSession(sess)) goto on_error;
            break;
        case AC_WOWZA:
            if(!ac_WowzaOpenSession(sess, session_id)) goto on_error;
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            break;
    }
    return 1;
on_error:
    pu_log(LL_ERROR, "%s: Error on open session for device %d.", __FUNCTION__, sess->device);
    ac_close_session(sess);
    return 0;
}
void ac_close_session(t_at_rtsp_session* sess) {
    assert(sess);

    if(sess->url) {
        free(sess->url);
        sess->url = NULL;
    }
    switch(sess->device) {
        case AC_CAMERA:
            ac_alfaProCloseSession(sess);
             break;
        case AC_WOWZA:
            ac_WowzaCloseSession(sess);
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported device type %d", __FUNCTION__, sess->device);
            break;
    }
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
int ac_req_setup(t_at_rtsp_session* sess, int client_port) {
    assert(sess);

    switch(sess->device) {
        case AC_CAMERA:
            return ac_alfaProSetup(sess, client_port);
        case AC_WOWZA:
            return ac_WowzaSetup(sess, client_port);
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

