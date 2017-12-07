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

#include <curl/curl.h>
#include <memory.h>
#include <ctype.h>

#include "pu_logger.h"

#include "ag_defaults.h"

#include "ac_rtsp.h"

#define AC_RTSP_HEADER_SIZE         4097
#define AC_RTSP_BODY_SIZE           8193
#define AC_RTSP_TRANSPORT_SIZE      100
#define AC_RTSP_SESSION_ID_SIZE     20
#define AC_WOWZA_SESSION_ID_SIZE    30
#define AC_RTSP_TRACK_SIZE          20

#define AC_LOW_RES          "11"
#define AC_HI_RES           "12"

#define AC_TRACK            "trackID="
#define AC_PLAY_RANGE       "npt=0.000-"
#define AC_VIDEO_TRACK      0
#define AC_STREAMING_TCP    0


typedef struct {
    char *buf;
    size_t sz;
} t_ac_callback_buf;

typedef struct {
    CURL* h;
    char url[AC_RTSP_HEADER_SIZE];
    char track[AC_RTSP_TRACK_SIZE];
    char session_id[AC_RTSP_SESSION_ID_SIZE];
    char vs_session_id[AC_WOWZA_SESSION_ID_SIZE];
    t_ac_callback_buf req_body;
    t_ac_callback_buf ans_hdr;
    t_ac_callback_buf ans_body;
    struct curl_slist* slist;
} t_handler;

static t_handler cam = {0};
static t_handler vs = {0};

static int err_report(int res) {
    pu_log(LL_ERROR, "%s: Curl error. RC = %d", __FUNCTION__, res);
    return 0;
}

#define ERR_REPORT(a)  return err_report(a)

static size_t writer(void *ptr, size_t size, size_t nmemb, void *userp) {
    t_ac_callback_buf* dataToRead = (t_ac_callback_buf *)userp;
    char *data = (char *)ptr;
    if (dataToRead == NULL || dataToRead->buf == NULL) {
        pu_log(LL_ERROR, "Callback %s: dataToRead == NULL", __FUNCTION__);
        return 0;
    }
    /* keeping one byte for the null byte */
    if((strlen(dataToRead->buf)+(size * nmemb)) > (dataToRead->sz - 1)) {
        pu_log(LL_ERROR, "Callback %s - bufffer overflow. Got %d, but need %d Result truncated", __FUNCTION__, dataToRead->sz, (size * nmemb));
        return 0;
    }
    else {
        strncat(dataToRead->buf, data, (size * nmemb));
        return (size * nmemb);
    }
}
static size_t reader(void *ptr, size_t size, size_t nmemb, void *userp) {
    t_ac_callback_buf *dataToWrite = (t_ac_callback_buf *) userp;
    size_t dataWritten = 0;
    if (dataToWrite == NULL || dataToWrite->buf == NULL) {
        printf("read_callback: dataToWrite == NULL");
        return 0;
    }
    if (size * nmemb < 1) {
        printf("size * nmemb < 1");
        return 0;
    }
    if (dataToWrite->sz > 0) {
        if (dataToWrite->sz > (size * nmemb)) {
            dataWritten = size * nmemb;
        }
        else {
            dataWritten = (size_t)dataToWrite->sz;
        }
        memcpy (ptr, dataToWrite->buf, dataWritten);
        dataToWrite->buf += dataWritten;
        dataToWrite->sz -= dataWritten;
        if(dataToWrite->sz < 0) dataToWrite->sz = 0;

        return dataWritten; /* we return 1 byte at a time! */
    }
    return 0;
}

static void copy_result(t_handler hndlr, char* h, size_t hs, char* b, size_t bs) {
    strncpy(h, hndlr.ans_hdr.buf, hs); hndlr.ans_hdr.buf[0] = '\0';
    strncpy(b, hndlr.ans_body.buf, bs); hndlr.ans_body.buf[0] = '\0';
}
static const char* make_transport_string(char* buf, size_t size, int port, int tcp_streaming) {
    char s_port1[20];
    char s_port2[20];
    buf[0] = '\0';
    sprintf(s_port1, "%d", port);
    sprintf(s_port2, "%d", port+1);
    if(tcp_streaming)
        sprintf(buf, "RTP/AVP/TCP;unicast;client_port=%s-%s", s_port1, s_port2);
    else
        sprintf(buf, "RTP/AVP;unicast;client_port=%s-%s", s_port1, s_port2);
    return buf;
}
static t_handler* get_handler(t_ac_rtsp_device device_type) {
    switch(device_type) {
        case AC_CAMERA:
            return &cam;
            break;
        case AC_WOWZA:
            return &vs;
            break;
        default:
            pu_log(LL_ERROR, "%s Unknown device type", __FUNCTION__);
            return NULL;
    }
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
static char* get_sesion_id(const char* src, char* buf, size_t size) {
    int start_pos, len;
    buf[0] = '\0';
    if(start_pos = findNCSubstr(src, "Session: "), start_pos < 0) return buf;
    start_pos += strlen("Session: ");
    if(len = findNCSubstr(src+start_pos, ";"), len < 0) return buf;
    memcpy(buf, src + start_pos, (size_t)len);
    buf[len] = '\0';
    return buf;
}

int ac_rtsp_init() {
    CURLcode res;;

    if(res = curl_global_init(CURL_GLOBAL_ALL), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Error in curl_global_init. RC = %d", res);
        return 0;
    }
    return 1;
}
void ac_rtsp_down() {
    curl_global_cleanup();
}

int ac_open_session(t_ac_rtsp_device device_type, const char* url) {
    t_handler* h = get_handler(device_type);
    if(!h) return -1;

    h->slist = NULL;

    if(!url || !strlen(url)) {
        pu_log(LL_ERROR, "%s: Device URL is NULL or empty");
        return 0;
    }
    if(h->req_body.buf = malloc(AC_RTSP_BODY_SIZE), !h->req_body.buf) {
        pu_log(LL_ERROR, "%s Memory allocation error", __FUNCTION__);
        return 0;
    }

    if(h->ans_hdr.buf = malloc(AC_RTSP_HEADER_SIZE), !h->ans_hdr.buf) {
        pu_log(LL_ERROR, "%s Memory allocation error", __FUNCTION__);
        free(h->req_body.buf);
        return 0;
    }
    if(h->ans_body.buf = malloc(AC_RTSP_BODY_SIZE), !h->ans_body.buf) {
        pu_log(LL_ERROR, "%s Memory allocation error", __FUNCTION__);
        free(h->req_body.buf);
        free(h->ans_hdr.buf);
        return 0;
    }
    h->ans_hdr.sz = AC_RTSP_HEADER_SIZE;
    h->ans_body.sz = AC_RTSP_BODY_SIZE;
    h->req_body.sz = AC_RTSP_BODY_SIZE;

    if(h->h = curl_easy_init(), !h->h) {
        pu_log(LL_ERROR, "%s: curl_easy_init.");
        return 0;
    }

    CURLcode res;
    if(res = curl_easy_setopt(h->h, CURLOPT_URL, url), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(h->h, CURLOPT_HTTPAUTH, 0L), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(h->h, CURLOPT_HEADERDATA, &h->ans_hdr), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(h->h, CURLOPT_HEADERFUNCTION, writer), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(h->h, CURLOPT_READFUNCTION, reader), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(h->h, CURLOPT_READDATA, &h->req_body), res != CURLE_OK) goto on_error;


    if (res = curl_easy_setopt(h->h, CURLOPT_WRITEFUNCTION, writer), res != CURLE_OK) goto on_error;
    if (res = curl_easy_setopt(h->h, CURLOPT_WRITEDATA, &h->ans_body), res != CURLE_OK) goto on_error;
    if (res = curl_easy_setopt(h->h, CURLOPT_BUFFERSIZE, AC_RTSP_BODY_SIZE), res != CURLE_OK) goto on_error;

    return 1;
on_error:
    pu_log(LL_ERROR, "%s: curl_easy_setopt error. RC = %d", __FUNCTION__, res);
    ac_close_session(device_type);
    return 0;
}
void ac_close_session(t_ac_rtsp_device device_type) {
    t_handler* h = get_handler(device_type);
    if(!h) return;

    if(h->h) curl_easy_cleanup(h->h);
    free(h->ans_hdr.buf);
    free(h->ans_body.buf);
    free(h->req_body.buf);
    if(h->slist) curl_slist_free_all(h->slist);
}
int ac_req_options(t_ac_rtsp_device device_type, char* head, size_t h_size, char* body, size_t b_size) {
    CURLcode res = CURLE_OK;

    t_handler* h = get_handler(device_type);
    if(!h) return -1;

    if(res = curl_easy_setopt(h->h, CURLOPT_RTSP_STREAM_URI, h->url), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(h->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(h->h);
    if (res = curl_easy_setopt(h->h, CURLOPT_WRITEDATA, &h->ans_body), res != CURLE_OK) goto on_error;

    copy_result(*h, head, h_size, body, b_size);
    return 1;
on_error:
    ERR_REPORT(res);
}
int ac_req_setup(t_ac_rtsp_device device_type, char* head, size_t h_size, char* body, size_t b_size, int cient_port) {
    CURLcode res = CURLE_OK;

    t_handler* h = get_handler(device_type);
    if(!h) return -1;

    char transport[AC_RTSP_TRANSPORT_SIZE];
    char uri[AC_RTSP_HEADER_SIZE];

    sprintf(h->track, "%s%d", AC_TRACK, AC_VIDEO_TRACK);       /* save "trackID=0" for use in PLAY command */

    strcpy(uri, h->url);
    sprintf(uri+strlen(uri), "/%s", h->track);                 /* Add to url "/trackID=0" */

    make_transport_string(transport, sizeof(transport), cient_port, AC_STREAMING_TCP);

    if(res = curl_easy_setopt(h->h, CURLOPT_RTSP_STREAM_URI, uri), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(h->h, CURLOPT_RTSP_TRANSPORT, transport), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(h->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(h->h);
    if (res = curl_easy_setopt(h->h, CURLOPT_WRITEDATA, &cam.ans_body), res != CURLE_OK) goto on_error;

    get_sesion_id(h->ans_hdr.buf, cam.session_id, AC_RTSP_SESSION_ID_SIZE);    /* Save session_id to use in PLAY command */

    copy_result(*h, head, h_size, body, b_size);
    return 1;
on_error:
    ERR_REPORT(res);
}
int ac_req_play(t_ac_rtsp_device device_type, char* head, size_t h_size, char* body, size_t b_size) {
    CURLcode res = CURLE_OK;

    t_handler* h = get_handler(device_type);
    if(!h) return -1;

    char uri[AC_RTSP_HEADER_SIZE];
    strcpy(uri, h->url);
    sprintf(uri+strlen(uri), "/%s", h->track);                 /* Add to url "/trackID=0" */

    if(res = curl_easy_setopt(h->h, CURLOPT_RTSP_STREAM_URI, uri), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(h->h, CURLOPT_RTSP_SESSION_ID, h->session_id), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(h->h, CURLOPT_RANGE, AC_PLAY_RANGE), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(h->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(h->h);
    if (res = curl_easy_setopt(h->h, CURLOPT_WRITEDATA, &h->ans_body), res != CURLE_OK) goto on_error;
    curl_easy_setopt(h->h, CURLOPT_RANGE, NULL);

    copy_result(cam, head, h_size, body, b_size);
    return 1;
on_error:
    ERR_REPORT(res);
}
int ac_req_teardown(t_ac_rtsp_device device_type, char* head, size_t h_size, char* body, size_t b_size) {
    CURLcode res = CURLE_OK;

    t_handler* h = get_handler(device_type);
    if(!h) return -1;

    if (res = curl_easy_setopt(h->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN), res != CURLE_OK) goto on_error;
    res = curl_easy_perform(h->h);
    if (res = curl_easy_setopt(h->h, CURLOPT_WRITEDATA, &h->ans_body), res != CURLE_OK) goto on_error;

    copy_result(cam, head, h_size, body, b_size);
    return 1;
on_error:
    ERR_REPORT(res);
}

int ac_req_cam_describe(char* head, size_t h_size, char* body, size_t b_size) {
    CURLcode res = CURLE_OK;

    if(res = curl_easy_setopt(cam.h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(cam.h);
    if (res = curl_easy_setopt(cam.h, CURLOPT_WRITEDATA, &cam.ans_body), res != CURLE_OK) goto on_error;

    copy_result(cam, head, h_size, body, b_size);
    return 1;
on_error:
    ERR_REPORT(res);
}
/* [login:password@]ip:port/resolution/ */
const char* ac_makeCamURL(char *url, size_t size, const char* ip, int port, const char* login, const char* pwd, t_ao_cam_res resolution) {
    char s_port[20];
    url[0] = '\0';
    sprintf(s_port, "%d", port);

    if((strlen(s_port) + strlen(ip) + strlen(login) + strlen(pwd) + strlen(AC_LOW_RES) + 4) > (size -1)) {
        pu_log(LL_ERROR, "%s: uri size too small!", __FUNCTION__);
        return url;
    }

    if(strlen(login) && strlen(pwd)) {
        strcpy(url, login);
        strcat(url, ":");
        strcat(url, pwd);
        strcat(url, "@");
        strcat(url, ip);
    }
    else {
        strcpy(url, ip);
    }
    strcat(url, ":");
    strcat(url, s_port);
    strcat(url, "/");
    switch (resolution) {
        case AO_RES_LO:
            strcat(url, AC_LOW_RES);
            break;
        case AO_RES_HI:
            strcat(url, AC_HI_RES);
            break;
        default:
            break;
    }
    return url;
}


/********************************************** Video Server part *****************************************************/
int ac_req_vs_announce1(char* cam_describe_body, char* head, size_t h_size, char* body, size_t b_size) {
    CURLcode res = CURLE_OK;

    strcpy(vs.req_body.buf, cam_describe_body);
    vs.req_body.sz = strlen(cam_describe_body);

    char buf[30];
    snprintf(buf, sizeof(buf) - 1, "Content-Length: %lu", vs.req_body.sz);
    vs.slist = curl_slist_append(vs.slist, buf);

    if(res = curl_easy_setopt(vs.h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_ANNOUNCE), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(vs.h);
    if (res = curl_easy_setopt(vs.h, CURLOPT_WRITEDATA, &vs.ans_body), res != CURLE_OK) goto on_error;

    get_sesion_id(vs.ans_hdr.buf, vs.session_id, AC_RTSP_SESSION_ID_SIZE);    /* Save session_id to use in ANNOUNCE2 command */

    copy_result(vs, head, h_size, body, b_size);
    return 1;
on_error:
    ERR_REPORT(res);
}
int ac_req_vs_announce2(char* head, size_t h_size, char* body, size_t b_size) {
    CURLcode res = CURLE_OK;

    if(res = curl_easy_setopt(vs.h, CURLOPT_RTSP_SESSION_ID, vs.session_id), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(vs.h, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(vs.h, CURLOPT_USERNAME,vs.session_id), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(vs.h);
    if (res = curl_easy_setopt(vs.h, CURLOPT_WRITEDATA, &vs.ans_body), res != CURLE_OK) goto on_error;

    copy_result(vs, head, h_size, body, b_size);
    return 1;
on_error:
    ERR_REPORT(res);
}
/* <vs_url>:<port>/ppcvideoserver/<vs_session_id> */
const char* ac_makeVSURL(char *url, size_t size, const char* vs_url, int port, const char* vs_session_id) {
    char s_port[20];
    sprintf(s_port, "%d", port);
    strcpy(vs.vs_session_id, vs_session_id);
    url[0] = '\0';

    sprintf(url, "%s:%s/%s/%s", vs_url, s_port, DEFAULT_PPC_VIDEO_FOLDER, vs_session_id);
    return url;
}