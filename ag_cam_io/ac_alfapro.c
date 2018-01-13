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
 Created by gsg on 10/01/18.
*/

#include <string.h>
#include <curl/curl.h>

#include "pu_logger.h"

#include "au_string.h"
#include "ac_http.h"


#include "ac_alfapro.h"

#define AC_LOW_RES          "11"
#define AC_HI_RES           "12"

#define AC_SERVER_PORT      "server_port="

typedef struct {
    CURL* h;
    char track[AC_RTSP_TRACK_SIZE];
} t_curl_session;


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
/*
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

        return dataWritten; // we return 1 byte at a time!
    }
    return 0;
}
*/
static int err_report(int res) {
    pu_log(LL_ERROR, "%s: Curl error. RC = %d", __FUNCTION__, res);
    return 0;
}
/*
static int ac_get_server_port(const char* msg) {
    char number [10]={0};
    ssize_t pos;

    if(pos = au_findSubstr(msg, AC_SERVER_PORT, AU_NOCASE), pos < 0) return -1;
    if(au_getNumber(number, sizeof(number), msg+pos+strlen(AC_SERVER_PORT))) return atoi(number);

    return -1;
}
*/
static const char* make_transport_string(char* buf, size_t size, int port, int tcp_streaming) {
    char s_port1[20];
    char s_port2[20];
    buf[0] = '\0';
    sprintf(s_port1, "%d", port);
    sprintf(s_port2, "%d", port+1);

    if(tcp_streaming) {
        if (!au_strcpy(buf, "RTP/AVP/TCP;unicast;client_port=", size)) return NULL;
    }
    else {
        if (!au_strcpy(buf, "RTP/AVP/UDP;unicast;client_port=", size)) return NULL;
    }

    if(!au_strcat(buf, s_port1, size)) return NULL;
    if(!au_strcat(buf, "-", size)) return NULL;
    if(!au_strcat(buf, s_port2, size)) return NULL;

    return buf;
}

/*************************************************************************/
t_at_rtsp_session* ac_alfaProInit() {
    CURLcode res;

    t_at_rtsp_session* ret = calloc(sizeof(t_at_rtsp_session), 1);
    if(!ret) {
        pu_log(LL_ERROR, "%s: Memory allocation error on %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    ret->device = AC_CAMERA;

    if(res = curl_global_init(CURL_GLOBAL_ALL), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Error in curl_global_init. RC = %d", res);
        goto on_error;
    }
    return ret;
on_error:
    if(ret) free(ret);
    return NULL;
}

void ac_alfaProDown(t_at_rtsp_session* sess) {
    AT_DT_NR(sess->device, AC_CAMERA);
    curl_global_cleanup();
    free(sess);
}

int ac_alfaProOpenSession(t_at_rtsp_session* sess) {
    t_curl_session *cs = NULL;
    AT_DT_RT(sess->device, AC_CAMERA, 0);

    if(cs = calloc(sizeof(t_curl_session),1), !cs) {
        pu_log(LL_ERROR, "%s: Memory allocation error", __FUNCTION__);
        return 0;
    }
    if(cs->h = curl_easy_init(), !cs->h) {
        pu_log(LL_ERROR, "%s: curl_easy_init.", __FUNCTION__);
        free(cs);
        return 0;
    }

    sess->session = cs;
    CURLcode res;

    curl_easy_setopt(cs->h, CURLOPT_VERBOSE, 1L);
    if(res = curl_easy_setopt(cs->h, CURLOPT_URL, sess->url), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(cs->h, CURLOPT_HTTPAUTH, 0L), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(cs->h, CURLOPT_TCP_KEEPALIVE, 1L), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERFUNCTION, writer), res != CURLE_OK) goto on_error;

    if (res = curl_easy_setopt(cs->h, CURLOPT_WRITEFUNCTION, writer), res != CURLE_OK) goto on_error;

    return 1;
on_error:
    free(cs);
    sess->session = NULL;
    pu_log(LL_ERROR, "%s: Errors on curl_easy_setopt. RC = %d", __FUNCTION__, res);
    return 0;
}

void ac_alfaProCloseSession(t_at_rtsp_session* sess) {
    AT_DT_NR(sess->device, AC_CAMERA);
    t_curl_session* cs = sess->session;
    if (cs->h) curl_easy_cleanup(cs->h);
    free(cs);
    sess->session = NULL;
}

int ac_alfaProOptions(t_at_rtsp_session* sess) {
    char head[200]={0};
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    t_ac_callback_buf header_buf = {head, sizeof(head)};

    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERDATA, &header_buf), res != CURLE_OK) return err_report(res);
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, sess->url), res != CURLE_OK) return err_report(res);
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS), res != CURLE_OK) return err_report(res);

    res = curl_easy_perform(cs->h);
    if(ac_http_analyze_perform(res, cs->h,  __FUNCTION__) != CURLE_OK) return err_report(res);

    pu_log(LL_INFO, "%s: Result = \n%s", __FUNCTION__, head);

    return 1;
}

int ac_alfaProDescribe(t_at_rtsp_session* sess, char* descr, size_t size) {

    char header[1000] = {0};
    t_curl_session* cs = sess->session;
    CURLcode res = CURLE_OK;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    descr[0] = '\0';
    t_ac_callback_buf body_buf = {descr, size};
    t_ac_callback_buf header_buf = {header, sizeof(header)};

    if (res = curl_easy_setopt(cs->h, CURLOPT_WRITEDATA, &body_buf), res != CURLE_OK) return err_report(res);
    if (res = curl_easy_setopt(cs->h, CURLOPT_BUFFERSIZE, size), res != CURLE_OK) return err_report(res);

    if (res = curl_easy_setopt(cs->h, CURLOPT_HEADERDATA, &header_buf), res != CURLE_OK) return err_report(res);

    if (res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, sess->url), res != CURLE_OK) return err_report(res);
    if (res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long) CURL_RTSPREQ_DESCRIBE), res != CURLE_OK) return err_report(res);

    res = curl_easy_perform(cs->h);
    if (ac_http_analyze_perform(res, cs->h, __FUNCTION__) != CURLE_OK) return err_report(res);

    pu_log(LL_INFO, "%s: Header = \n%sBody = \n%s", __FUNCTION__, header, descr);

    return 1;
}

int ac_alfaProSetup(t_at_rtsp_session* sess, int client_port) {
    char header[1000] = {0};
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    t_ac_callback_buf header_buf = {header, sizeof(header)};

    char transport[AC_RTSP_TRANSPORT_SIZE];
    char uri[AC_RTSP_HEADER_SIZE];

    if(!au_strcpy(cs->track, AC_TRACK, sizeof(cs->track))) return err_report(res);
    if(!au_strcat(cs->track, AC_VIDEO_TRACK, sizeof(cs->track))) return err_report(res); /* save "trackID=0" for use in PLAY command */


    if(!au_strcpy(uri, sess->url, sizeof(uri))) return err_report(res);
    if(!au_strcat(uri, "/", sizeof(uri))) return err_report(res);
    if(!au_strcat(uri, cs->track, sizeof(uri))) return err_report(res);                  /* Add to url "/trackID=0" */

    make_transport_string(transport, sizeof(transport), client_port, AC_STREAMING_UDP);

    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERDATA, &header_buf), res != CURLE_OK) return err_report(res);

    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, uri), res != CURLE_OK) return err_report(res);
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_TRANSPORT, transport), res != CURLE_OK) return err_report(res);
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP), res != CURLE_OK) return err_report(res);

    res = curl_easy_perform(cs->h);
    if(ac_http_analyze_perform(res, cs->h, __FUNCTION__) != CURLE_OK) return err_report(res);

    pu_log(LL_INFO, "%s: Header = \n%s", __FUNCTION__, header);

    return 1;
}

int ac_alfaProPlay(t_at_rtsp_session* sess) {
    char head[1000] = {0};
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    t_ac_callback_buf header_buf = {head, sizeof(head)};

    char uri[AC_RTSP_HEADER_SIZE];
    if(!au_strcpy(uri, sess->url, sizeof(uri))) return 0;
    if(!au_strcat(uri, "/", sizeof(uri))) return 0;
    if(!au_strcat(uri, cs->track, sizeof(uri))) return 0;  /* Add to url "/trackID=0" */

    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERDATA, &header_buf), res != CURLE_OK) return err_report(res);;

    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, uri), res != CURLE_OK) return err_report(res);
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_SESSION_ID, sess->rtsp_session_id), res != CURLE_OK) return err_report(res);
    if(res = curl_easy_setopt(cs->h, CURLOPT_RANGE, AC_PLAY_RANGE), res != CURLE_OK) return err_report(res);
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY), res != CURLE_OK) return err_report(res);

    res = curl_easy_perform(cs->h);
    if(ac_http_analyze_perform(res, cs->h,  __FUNCTION__) != CURLE_OK) return err_report(res);

    curl_easy_setopt(cs->h, CURLOPT_RANGE, NULL);

    pu_log(LL_INFO, "%s: Result = \n%s", __FUNCTION__, head);

    return 1;
}

int ac_alfaProTeardown(t_at_rtsp_session* sess) {
    char head[1000]= {0};
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    t_ac_callback_buf header_buf = {head, sizeof(head)};

    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERDATA, &header_buf), res != CURLE_OK) return err_report(res);

    if (res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN), res != CURLE_OK) return err_report(res);
    res = curl_easy_perform(cs->h);
    if(ac_http_analyze_perform(res, cs->h, __FUNCTION__) != CURLE_OK) return err_report(res);

    pu_log(LL_INFO, "%s: Result = \n%s", __FUNCTION__, head);

    return 1;
}

const char* ac_makeAlfaProURL(char *url, size_t size, const char* ip, int port, const char* login, const char* pwd, t_ao_cam_res resolution) {
    char s_port[20];
    url[0] = '\0';
    sprintf(s_port, "%d", port);

    if((strlen(s_port) + strlen(ip) + strlen(login) + strlen(pwd) + strlen(AC_LOW_RES) + 4) > (size -1)) {
        pu_log(LL_ERROR, "%s: uri size too small!", __FUNCTION__);
        return url;
    }

    if(strlen(login) && strlen(pwd)) {
        if(!au_strcpy(url, login, size)) return 0;
        if(!au_strcat(url, ":", size)) return 0;
        if(!au_strcat(url, pwd, size)) return 0;
        if(!au_strcat(url, "@", size)) return 0;
        if(!au_strcat(url, ip, size)) return 0;
    }
    else {
        if(!au_strcpy(url, ip, size)) return 0;
    }
    if(!au_strcat(url, ":", size)) return 0;
    if(!au_strcat(url, s_port, size)) return 0;
    if(!au_strcat(url, "/", size)) return 0;
    switch (resolution) {
        case AO_RES_LO:
            if(!au_strcat(url, AC_LOW_RES, size)) return 0;
            break;
        case AO_RES_HI:
            if(!au_strcat(url, AC_HI_RES, size)) return 0;
            break;
        default:
            break;
    }
    return url;
}
