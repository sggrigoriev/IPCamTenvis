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

#define AO_URI_START         "rtsp://"
#define AO_PORT_DELIM       ':'
#define AO_DELIMS           " \r\n"

#define AO_LOW_RES          "11"
#define AO_HI_RES           "12"

typedef struct {
    int found;
    unsigned int start;
    unsigned int len;
} t_ao_pos;


/* Find first occurence of digits from the start, return converted int. */
static int getNumber(const char* msg);

/* Copy to buf the furst numbers occuriencce */
static const char* getStrNumber(char* buf, size_t size, const char* msg);

/* Get the index of the first byte of IP:port address */
static t_ao_pos findURI(const char* msg);

static const char* strNCstr(const char* msg, const char* subs);
static int findNCSubstr(const char* msg, const char* subs);
static int findOneOfChars(const char* msg, const char* set);
static int max(int a, int b);
static int min(int a, int b);
static int get_min(int a, int b);

/* [login:password@]ip:port/resolution/ */
const char* ao_makeURI(char *uri, size_t size, const char* ip, int port, const char* login, const char* pwd, t_ao_cam_res resolution) {
    char s_port[20];
    sprintf(s_port, "%d", port);

    if(strlen(login) && strlen(pwd)) {
        strcpy(uri, login);
        strcat(uri, ":");
        strcat(uri, pwd);
        strcat(uri, "@");
        strcat(uri, ip);
    }
    else {
        strcpy(uri, ip);
    }
    strcat(uri, ":");
    strcat(uri, s_port);
    strcat(uri, "/");
    switch (resolution) {
        case AO_RES_LO:
            strcat(uri, AO_LOW_RES);
            break;
        case AO_RES_HI:
            strcat(uri, AO_HI_RES);
            break;
        default:
            break;
    }
    strcat(uri, "/");
    return uri;
}
void ao_get_uri(char* uri, size_t size, const char* msg) {
    uri[0] = '\0';
    t_ao_pos pos = findURI(msg);
    if(!pos.found) return;
    memcpy(uri, msg+pos.start, pos.len);
    uri[pos.len] ='\0';
}
void ao_cam_replace_uri(char* msg, size_t size, const char* new_uri) {
    char* str = malloc(size);
    t_ao_pos pos;
    unsigned int s_start = 0;
    unsigned int m_start = 0;

    memset(str, 0, size);

    while(pos = findURI(msg+m_start), pos.found) {
        memcpy(str+s_start, msg+m_start, pos.start); s_start += pos.start;                  /* Copy before URI */
        memcpy(str+s_start, new_uri, strlen(new_uri)); s_start += strlen(new_uri);          /* put IP:port */
        m_start += (pos.start+pos.len);
    }
    if(m_start < strlen(msg)) {
        memcpy(str+s_start, msg+m_start, strlen(msg) - m_start); s_start += (strlen(msg) - m_start);
    }
    str[s_start] = '\0';
    strcpy(msg, str);
    free(str);
}

t_ac_rtsp_type ao_get_msg_type(const char* msg) {
    if(strNCstr(msg, AO_RTSP_DESCRIBE)) return AC_DESCRIBE;
    if(strNCstr(msg, AO_RTSP_ANNOUNCE)) return AC_ANNOUNCE;
    if(strNCstr(msg, AO_RTSP_SETUP)) return AC_SETUP;
    if(strNCstr(msg, AO_RTSP_PLAY)) return AC_PLAY;
    if(strNCstr(msg, AO_RTSP_TEARDOWN)) return AC_TEARDOWN;
    return AC_UNDEFINED;

}
int ao_get_msg_number(const char* msg) {
    return getNumber(strNCstr(msg, AO_RTSP_REQ_NUMBER));
}
int ao_get_client_port(const char* msg) {
    return getNumber(strNCstr(msg, AO_CLIENT_PORT));
}
int ao_get_server_port(const char* msg) {
    return getNumber(strNCstr(msg, AO_SERVER_PORT));
}

int ao_cam_encode(t_ao_msg data, const char* to_cam_msg, size_t size) {
    return 0;
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

static t_ao_pos findURI(const char* msg) {
    t_ao_pos ret = {0};

    int start_pos = findNCSubstr(msg, AO_URI_START);
    if (start_pos < 0) return ret;
    ret.start = (unsigned int) start_pos + (unsigned int) strlen(AO_URI_START);
    int length1 = findOneOfChars(msg + ret.start, AO_DELIMS);
    int length2 = findNCSubstr(msg + ret.start, AO_TRACK_ID);
    int length = get_min(length1, length2);
    if(length < 0) return ret;
    ret.found = 1;
    ret.len = (unsigned int)length;
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

static int findOneOfChars(const char* msg, const char* set) {
    int i;
    for(i = 0; i < strlen(msg); i++) {
        unsigned int j;
        for(j = 0; j < strlen(set); j++) {
            if(tolower(msg[i]) == tolower(set[j])) return i;
        }
    }
    return -1;
}

static int max(int a, int b) {
    return (a > b)?a:b;
}
static int min(int a, int b) {
    return (a > b)?b:a;
}
static int get_min(int a, int b) {
    if(max(a,b) < 0 ) return -1;
    if(min(a,b) < 0) return max(a,b);
    return min(a,b);
}