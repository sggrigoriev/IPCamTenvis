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
#include "lib_tcp.h"

#include "au_string.h"
#include "ac_http.h"
#include "ag_settings.h"

#include "ac_alfapro.h"
#include "ac_cam_types.h"

#define AC_LOW_RES          "11"
#define AC_HI_RES           "12"

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

static int get_video_port(const char* msg) {
    int pos =  au_findSubstr(msg, AC_RTSP_SERVER_PORT, AU_NOCASE);
    if(pos < 0) return 0;
    char num[20] = {0};
    au_getNumber(num, sizeof(num), msg+pos+strlen(AC_RTSP_SERVER_PORT));
    if(!strlen(num)) return 0;

    return atoi(num);
}
static const char* get_source_ip(char* ip, size_t size, const char* msg) {
    int pos = au_findSubstr(msg, AC_RTSP_SOURCE_IP, AU_NOCASE);
    ip[0] = '\0';
    if(pos < 0) return ip;
    int start = pos + strlen(AC_RTSP_SOURCE_IP);
    int finish = au_findFirstOutOfSet(msg+start, "0123456789.") + start;
    if(finish < 0) return ip;
    if((finish - start+1) > size) return ip;
    memcpy(ip, msg+start, finish-start);
    ip[finish-start] = '\0';
    return ip;
}

/*************************************************************************/
int ac_alfaProInit(t_at_rtsp_session* sess) {
    AT_DT_RT(sess->device, AC_CAMERA, 0);

    CURLcode res = CURLE_OK;
    t_curl_session *cs = NULL;

    if(cs = calloc(sizeof(t_curl_session),1), !cs) {
        pu_log(LL_ERROR, "%s: Memory allocation error", __FUNCTION__);
        goto on_error;
    }
    if(cs->h = curl_easy_init(), !cs->h) {
        pu_log(LL_ERROR, "%s: curl_easy_init.", __FUNCTION__);
        goto on_error;
    }

    sess->session = cs;

    curl_easy_setopt(cs->h, CURLOPT_VERBOSE, 1L);
    if(res = curl_easy_setopt(cs->h, CURLOPT_URL, sess->url), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(cs->h, CURLOPT_HTTPAUTH, 0L), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(cs->h, CURLOPT_TCP_KEEPALIVE, 1L), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERFUNCTION, writer), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(cs->h, CURLOPT_WRITEFUNCTION, writer), res != CURLE_OK) err_report(res);
    if(strlen(ag_getCurloptCAInfo())) {
        if(res = curl_easy_setopt(cs->h, CURLOPT_CAINFO, ag_getCurloptCAInfo()), res != CURLE_OK) err_report(res);
    }
    if(res = curl_easy_setopt(cs->h, CURLOPT_SSL_VERIFYPEER, (long)ag_getCurloptSSLVerifyPeer()), res != CURLE_OK) err_report(res);

    return 1;
on_error:
    if(cs) {
        if (cs->h) {
            curl_easy_cleanup(cs->h);
            sess->session = NULL;
            pu_log(LL_ERROR, "%s: Errors on curl_easy_setopt. RC = %d", __FUNCTION__, res);
        }
        free(cs);
    }
    return 0;
}

void ac_alfaProDown(t_at_rtsp_session* sess) {
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

int ac_alfaProSetup(t_at_rtsp_session* sess, int media_type) {
    char header[1000] = {0};
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    t_ac_callback_buf header_buf = {header, sizeof(header)};

    char transport[AC_RTSP_TRANSPORT_SIZE];
    char uri[AC_RTSP_HEADER_SIZE];

    if(!au_strcpy(cs->track, AC_TRACK, sizeof(cs->track))) return err_report(res);
    if(!au_strcat(cs->track, (media_type==AC_ALFA_VIDEO_SETUP)?AC_VIDEO_TRACK:AC_AUDIO_TRACK, sizeof(cs->track))) return err_report(res); /* save "trackID=0" for use in PLAY command */


    if(!au_strcpy(uri, sess->url, sizeof(uri))) return err_report(res);
    if(!au_strcat(uri, "/", sizeof(uri))) return err_report(res);
    if(!au_strcat(uri, cs->track, sizeof(uri))) return err_report(res);                  /* Add to url "/trackID=0" */

    make_transport_string(transport, sizeof(transport), (media_type == AC_ALFA_VIDEO_SETUP)?sess->video_pair.dst.port.rtp:sess->audio_pair.dst.port.rtp, AC_STREAMING_UDP);

    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERDATA, &header_buf), res != CURLE_OK) return err_report(res);

    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, uri), res != CURLE_OK) return err_report(res);
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_TRANSPORT, transport), res != CURLE_OK) return err_report(res);
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP), res != CURLE_OK) return err_report(res);

    res = curl_easy_perform(cs->h);
    if(ac_http_analyze_perform(res, cs->h, __FUNCTION__) != CURLE_OK) return err_report(res);

    pu_log(LL_INFO, "%s: Header = \n%s", __FUNCTION__, header);

    int port = get_video_port(header);
    if(!port) {
        pu_log(LL_ERROR, "%s: Can not get media port from camera transport string", __FUNCTION__);
        return 0;
    }

    char lip[16] = {0};
    get_source_ip(lip, sizeof(lip), header);
    char* ipd = strdup(strlen(lip)?lip:ag_getCamIP());
    if(!ipd) {
        pu_log(LL_ERROR, "%s: Memory allocation error ar %d", __FUNCTION__, __LINE__);
        return 0;
    }
    if(media_type == AC_ALFA_VIDEO_SETUP) {
        sess->video_pair.src.port.rtp = port;
        sess->video_pair.src.port.rtcp = port+1;
        sess->video_pair.src.ip = ipd;
    }
    else {  /* AC_ALFA_AUDIO_SETUP */
        sess->audio_pair.src.port.rtp = port;
        sess->audio_pair.src.port.rtcp = port+1;
        sess->audio_pair.src.ip = ipd;
    }
    return 1;
}

int ac_alfaProPlay(t_at_rtsp_session* sess) {
    char head[1000] = {0};
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    t_ac_callback_buf header_buf = {head, sizeof(head)};

    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERDATA, &header_buf), res != CURLE_OK) return err_report(res);;

    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, sess->url), res != CURLE_OK) return err_report(res);
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
    strcpy(url, "rtsp://");
    if(strlen(login) && strlen(pwd)) {
        if(!au_strcat(url, login, size)) return 0;
        if(!au_strcat(url, ":", size)) return 0;
        if(!au_strcat(url, pwd, size)) return 0;
        if(!au_strcat(url, "@", size)) return 0;
        if(!au_strcat(url, ip, size)) return 0;
    }
    else {
        if(!au_strcat(url, ip, size)) return 0;
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
