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
 Created by gsg on 30/10/17.
*/
#include <string.h>
#include <ctype.h>

#include "pu_logger.h"

#include "ao_cma_cam.h"

#define AO_RTSP_DESCRIBE    "DESCRIBE"
#define AO_RTSP_ANNOUNCE    "ANNOUNCE"
#define AO_RTSP_SETUP       "SETUP"
#define AO_RTSP_PLAY        "PLAY"
#define AO_RTSP_TEARDOWN    "TEARDOWN"

#define AO_RTSP_REQ_NUMBER  "CSeq:"
#define AO_TRACK_ID         "trackID="
#define AO_CLIENT_PORT      "client_port"
#define AO_SERVER_PORT      "server_port"
#define AO_SESSION_ID       "Session:"

#define AO_IP_START         "rtsp://"
#define AO_PORT_START       ':'

static t_ac_rtsp_type get_msg_type(const char* msg);
static int calc_tracks(const char* msg);

/* Find first occurence of digits from the start, return converted int. */
static int getNumber(const char* msg);

/* Copy to buf the furst numbers occuriencce */
static const char* getStrNumber(char* buf, size_t size, const char* msg);

/* Get the index of the first byte of IP address */
static int getIP(const char* msg);

static const char* strNCstr(const char* msg, const char* subs);

static int findNCSubstr(const char* msg, const char* subs);
static int findChar(const char* msg, char c);

t_ac_rtsp_msg ao_cam_decode_req(const char* cam_message) {
    t_ac_rtsp_msg ret;

    ret.msg_type = get_msg_type(cam_message);
    ret.number = getNumber(strNCstr(cam_message, AO_RTSP_REQ_NUMBER));

    switch(ret.msg_type) {
        case AC_DESCRIBE:
            ret.b.describe.video_tracks_number = calc_tracks(cam_message);
            break;
        case AC_ANNOUNCE:
            ret.b.describe.video_tracks_number = calc_tracks(cam_message);
            break;
        case AC_SETUP:
            ret.b.setup.track_number = getNumber(strNCstr(cam_message, AO_TRACK_ID));
            ret.b.setup.client_port = getNumber(strNCstr(cam_message, AO_CLIENT_PORT));
            break;
        case AC_PLAY:
            getStrNumber(ret.b.play.session_id, sizeof(ret.b.play.session_id), strNCstr(cam_message, AO_SESSION_ID));
            break;
        default:
            break;
    }
    return ret;
}

t_ac_rtsp_msg ao_cam_decode_ans(t_ac_rtsp_type req_type, int req_number, const char* cam_message) {
    t_ac_rtsp_msg ret;

    ret.msg_type = req_type;
    ret.number = getNumber(strNCstr(cam_message, AO_RTSP_REQ_NUMBER));
    if(ret.number != req_number) {
        pu_log(LL_WARNING, "%s: Cam answer got different #! Request# = %d, Response# = %d in message %s", __FUNCTION__, req_number, ret.number, cam_message);
    }
    switch(ret.msg_type) {
        case AC_SETUP:
            ret.b.setup.server_port = getNumber(strNCstr(cam_message, AO_SERVER_PORT));
            break;
        default:
            break;
    }
    return ret;
}

const char* ao_cam_replace_addr(char* msg, size_t size) {
    char* str = malloc(size);
    int idx;
    while(idx = getIP(msg), idx >= 0) {
        memcpy(str, msg, idx);
        str[idx] = '\0';


    }
}

static t_ac_rtsp_type get_msg_type(const char* msg) {
    if(strNCstr(msg, AO_RTSP_DESCRIBE)) return AC_DESCRIBE;
    if(strNCstr(msg, AO_RTSP_ANNOUNCE)) return AC_ANNOUNCE;
    if(strNCstr(msg, AO_RTSP_SETUP)) return AC_SETUP;
    if(strNCstr(msg, AO_RTSP_PLAY)) return AC_PLAY;
    if(strNCstr(msg, AO_RTSP_TEARDOWN)) return AC_TEARDOWN;
    return AC_UNDEFINED;
}
static int calc_tracks(const char* msg) {
    int ret = 0;
    const char* ptr = msg;
    while(ptr=strNCstr(ptr, AO_TRACK_ID), ptr != NULL) ret++;
    return ret;
}

static int getNumber(const char* msg) {
    char buf[128];
    if(!msg) return 0;

    const char* num = getStrNumber(buf, sizeof(buf), msg);
    if(!strlen(buf)) return 0;

    return atoi(num);
}
static const char* getStrNumber(char* buf, size_t size, const char* msg) {
    unsigned int i;
    unsigned int counter = 0;
    buf[0] = '\0';
    if(!msg) return buf;

    for(i = 0; i < strlen(msg); i++) {
        if(isdigit(msg[i])) {
            buf[counter++] = msg[i];
        }
        else if(counter) {
            buf[counter] = '\0';
            break;
        }
    }
    return buf;
}

static const char* strNCstr(const char* msg, const char* subs) {
    int a = findNCSubstr(msg, subs);
    if(a < 0)
        return NULL;
    else
        return msg + a;
}
static int getIP(const char* msg) {
    int i = findNCSubstr

}
static int findChar(const char* msg, char c) {
    unsigned int i;
    for(i = 0; i < strlen(msg), i++)
        if(tolower(msg[i]) == tolower(c))
            return i;
    return -1;
}

static int findNCSubstr(const char* msg, const char* subs) {
    if(!msg || !subs) return -1;
    unsigned int i;
    int gotit = 0;
    for(i = 0; i < strlen(msg); i++) {
        if((strlen(msg)-i) >= strlen(subs)) {
            unsigned j;
            for(j = 0; j < strlen(subs); j++) {
                if(tolower(msg[i+j]) != tolower(subs[j])) {
                    gotit = 0;
                    break;
                }
                gotit = 1;
            }
            if(gotit) return i;
        }
        else return -1;
    }
    return -1;
}