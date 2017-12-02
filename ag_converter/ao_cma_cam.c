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
#define AO_PORT_DELIM       ':'

typedef struct {
    int found;
    unsigned int start;
    unsigned int len;
} t_ao_pos;

static t_ac_rtsp_type get_msg_type(const char* msg);
static int calc_tracks(const char* msg);
static const char* getIP_port(char* buf, size_t size, const char* msg);

/* Find first occurence of digits from the start, return converted int. */
static int getNumber(const char* msg);

/* Copy to buf the furst numbers occuriencce */
static const char* getStrNumber(char* buf, size_t size, const char* msg);

/* Get the index of the first byte of IP:port address */
static t_ao_pos findIP_port(const char* msg);

static const char* strNCstr(const char* msg, const char* subs);
static int findNCSubstr(const char* msg, const char* subs);
static int findFirstDigit(const char* msg);
static int findFirstNonDigit(const char* msg);
static int findChar(const char* msg, char c);

t_ac_rtsp_msg ao_cam_decode_req(const char* cam_message) {
    t_ac_rtsp_msg ret;

    ret.msg_type = get_msg_type(cam_message);
    ret.number = getNumber(strNCstr(cam_message, AO_RTSP_REQ_NUMBER));
    getIP_port(ret.ip_port, sizeof(ret.ip_port), cam_message);


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

const char* ao_cam_replace_addr(char* msg, size_t size, const char* ip_port) {
    char* str = malloc(size);
    t_ao_pos pos;
    unsigned int s_start = 0;
    unsigned int m_start = 0;

    memset(str, 0, size);

    while(pos = findIP_port(msg+m_start), pos.found) {
        memcpy(str+s_start, msg+m_start, pos.start); s_start += pos.start;                  /* Copy before IP */
        memcpy(str+s_start, ip_port, strlen(ip_port)); s_start += strlen(ip_port);          /* put IP:port */

        m_start += (pos.start+pos.len);
    }
    if(m_start < strlen(msg)) {
        memcpy(str+s_start, msg+m_start, strlen(msg) - m_start); s_start += (strlen(msg) - m_start);
    }
    str[s_start] = '\0';
    strcpy(msg, str);
    free(str);
    return msg;
}

const char* ao_makeIPPort(char* buf, size_t size, const char* ip, int port) {
    strcpy(buf, ip);
    snprintf(buf+strlen(buf),size-strlen(buf), ":%d", port);
    return buf;
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
static const char* getIP_port(char* buf, size_t size, const char* msg) {
    t_ao_pos pos = findIP_port(msg);
    if(!pos.found) buf[0] = '\0';
    memcpy(buf, msg+pos.start, pos.len);
    buf[pos.len] = '\0';
    return buf;
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
static t_ao_pos findIP_port(const char* msg) {  /* ip:port */
    t_ao_pos ret = {0};
    int start_ip_delim, start_addr, start_port_delim, start_non_addr;

    start_ip_delim = findNCSubstr(msg, AO_IP_START);
    if(start_ip_delim < 0) return ret;
    if(start_addr = findFirstDigit(msg+start_ip_delim), start_addr < 0) return ret;
    start_addr += start_ip_delim;                   /* got absolute index */
    start_port_delim = findChar(msg+start_addr, AO_PORT_DELIM);
    if(start_port_delim < 0) return ret;
    start_port_delim += start_addr;
    start_non_addr = findFirstNonDigit(msg+start_port_delim+1)+start_port_delim+1;
    ret.found = 1;
    ret.start = (unsigned int)start_addr;
    ret.len = (unsigned int)(start_non_addr - start_addr);
    return ret;
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

static int findFirstDigit(const char* msg) {
    unsigned int i;
    for(i = 0; i < strlen(msg); i++)
        if(isdigit(msg[i])) return i;
    return -1;
}

static int findFirstNonDigit(const char* msg) {
    unsigned int i;
    for(i = 0; i < strlen(msg); i++)
        if(!isdigit(msg[i])) return i;
    return (int)strlen(msg);
}

static int findChar(const char* msg, char c) {
    unsigned int i;
    for(i = 0; i < strlen(msg); i++)
        if(tolower(msg[i]) == tolower(c)) return i;
    return -1;
}