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

#include <memory.h>
#include <curl/curl.h>
#include <ctype.h>

#include "pu_logger.h"
#include "lib_tcp.h"

#include "au_string.h"
#include "ac_http.h"
#include "ag_settings.h"

#include "ac_alfapro.h"
#include "ac_cam_types.h"

#define AC_LOW_RES          "12"
#define AC_HI_RES           "11"
#define AC_HDR_ONLY         1
#define AC_HDR_WITH_BODY    0

typedef struct {
    CURL* h;
    t_ac_callback_buf header_buf;
    t_ac_callback_buf body_buf;
} t_curl_session;

static unsigned long AC_BUF_SIZE;

static size_t writer(void *ptr, size_t size, size_t nmemb, void *userp) {
    t_ac_callback_buf* dataToRead = (t_ac_callback_buf *)userp;

    if(!isalnum(*(char* )ptr) || (size * nmemb > AC_BUF_SIZE)) return size * nmemb; /* Smth else is using our callback */

    if (dataToRead == NULL || dataToRead->buf == NULL) {
        pu_log(LL_ERROR, "Callback %s: dataToRead == NULL", __FUNCTION__);
        return 0;
    }

    if(dataToRead->free_space > AC_BUF_SIZE) {
        pu_log(LL_ERROR, "Callback %s: dataToRead->free_space = %d", __FUNCTION__, dataToRead->free_space);
        return 0;
    }
    if(dataToRead->buf_sz > AC_BUF_SIZE) {
        pu_log(LL_ERROR, "Callback %s: dataToRead->buf_sz = %d", __FUNCTION__, dataToRead->buf_sz);
        return 0;
    }
    if(size * nmemb > dataToRead->free_space) {
        pu_log(LL_ERROR, "Callback %s - bufffer overflow. Got %d, but need %d Result ignored", __FUNCTION__, dataToRead->free_space, (size * nmemb));
        return 0;
    }
    else {
        memcpy(dataToRead->buf+(dataToRead->buf_sz-dataToRead->free_space), ptr, size * nmemb);
        dataToRead->free_space -= size * nmemb;
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

static void reset_curl_buffers(t_curl_session* cs) {
    cs->header_buf.buf_sz = AC_BUF_SIZE;
    cs->body_buf.buf_sz = AC_BUF_SIZE;

    cs->header_buf.free_space = AC_BUF_SIZE;
    cs->body_buf.free_space = AC_BUF_SIZE;

    bzero(cs->header_buf.buf, cs->header_buf.buf_sz);
    bzero(cs->body_buf.buf, cs->body_buf.buf_sz);
}

static const char* make_il_transport_string(char* buf, size_t size, int media_type) {
    buf[0] = '\0';

    if(!au_strcpy(buf, "RTP/AVP/TCP;unicast;", size)) return NULL;
    if(!au_strcat(buf, (media_type == AC_RTSP_VIDEO_SETUP)?AC_IL_VIDEO_PARAMS:AC_IL_AUDIO_PARAMS, size)) return NULL;

    return buf;
}
static const char* make_rt_transport_string(char* buf, size_t size, int port) {
    char s_port1[20];
    char s_port2[20];

    sprintf(s_port1, "%d", port);
    sprintf(s_port2, "%d", port+1);

    buf[0] = '\0';

    if(!au_strcpy(buf, "RTP/AVP/UDP;unicast;client_port=", size)) return NULL;
    if(!au_strcat(buf, s_port1, size)) return NULL;
    if(!au_strcat(buf, "-", size)) return NULL;
    if(!au_strcat(buf, s_port2, size)) return NULL;

    return buf;
}

static int get_media_port(const char* msg) {
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
    if (pos < 0) return ip;
    int start = pos + strlen(AC_RTSP_SOURCE_IP);
    int finish = au_findFirstOutOfSet(msg + start, "0123456789.") + start;
    if (finish < 0) return ip;
    if ((finish - start + 1) > size) return ip;
    memcpy(ip, msg + start, finish - start);
    ip[finish - start] = '\0';
    return ip;
}

/*************************************************************************/
int ac_alfaProInit(t_at_rtsp_session* sess) {
    AT_DT_RT(sess->device, AC_CAMERA, 0);

    AC_BUF_SIZE = ag_getStreamBufferSize();

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
    if(cs->header_buf.buf = calloc(AC_BUF_SIZE, 1), !cs->header_buf.buf) {
        pu_log(LL_ERROR, "%s: Memory alocation error at %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    if (cs->body_buf.buf = calloc(AC_BUF_SIZE, 1), !cs->body_buf.buf) {
        pu_log(LL_ERROR, "%s: Memory alocation error at %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    cs->body_buf.buf_sz = AC_BUF_SIZE;
    cs->body_buf.free_space = AC_BUF_SIZE;
    cs->header_buf.buf_sz = AC_BUF_SIZE;
    cs->header_buf.free_space = AC_BUF_SIZE;

    sess->session = cs;

#ifdef CURL_TRACE
    curl_easy_setopt(cs->h, CURLOPT_VERBOSE, 1L);
#endif
    if(res = curl_easy_setopt(cs->h, CURLOPT_URL, sess->url), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(cs->h, CURLOPT_HTTPAUTH, CURLAUTH_BASIC), res != CURLE_OK) goto on_error;
    if (res = curl_easy_setopt(cs->h, CURLOPT_USERNAME, ag_getCamLogin()), res != CURLE_OK) goto on_error;
    if (res = curl_easy_setopt(cs->h, CURLOPT_PASSWORD, ag_getCamPassword()), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(cs->h, CURLOPT_TCP_KEEPALIVE, 1L), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(cs->h, CURLOPT_WRITEFUNCTION, writer), res != CURLE_OK) goto on_error;
    if (res = curl_easy_setopt(cs->h, CURLOPT_WRITEDATA, &cs->body_buf), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERFUNCTION, writer), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(cs->h, CURLOPT_HEADERDATA, &cs->header_buf), res != CURLE_OK) goto on_error;


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
            pu_log(LL_ERROR, "%s: Errors on curl_easy_cl. RC = %d", __FUNCTION__, res);
        }
        if (cs->body_buf.buf) free(cs->body_buf.buf);
        if (cs->header_buf.buf) free(cs->header_buf.buf);
        free(cs);
    }
    return 0;
}

void ac_alfaProDown(t_at_rtsp_session* sess) {
    AT_DT_NR(sess->device, AC_CAMERA);

    t_curl_session* cs = sess->session;
    if(cs) {
        if (cs->h) {
            curl_easy_cleanup(cs->h);
            sess->session = NULL;
        }
        if (cs->body_buf.buf) free(cs->body_buf.buf);
        if (cs->header_buf.buf) free(cs->header_buf.buf);
        free(cs);
    }
    sess->session = NULL;
}

int ac_alfaProOptions(t_at_rtsp_session* sess, int suppress_info) {
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, sess->url), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(cs->h);

    if(ac_http_analyze_perform(res, cs->h,  __FUNCTION__) != CURLE_OK) goto on_error;
    if(!suppress_info)
        pu_log(LL_INFO, "%s: Result = \n%s", __FUNCTION__, cs->header_buf.buf);
    reset_curl_buffers(cs);

    return 1;
on_error:
    err_report(res);
    reset_curl_buffers(cs);
    return 0;
}

int ac_alfaProDescribe(t_at_rtsp_session* sess, char* descr, size_t size) {
    t_curl_session* cs = sess->session;
    CURLcode res = CURLE_OK;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    if (res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, sess->url), res != CURLE_OK) goto on_error;
    if (res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long) CURL_RTSPREQ_DESCRIBE), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(cs->h);

    if (ac_http_analyze_perform(res, cs->h, __FUNCTION__) != CURLE_OK) goto on_error;

    pu_log(LL_INFO, "%s: Header = \n%sBody = \n%s", __FUNCTION__, cs->header_buf.buf, cs->body_buf.buf);

    strncpy(descr, cs->body_buf.buf, size-1);
    descr[size-1] = '\0';
    reset_curl_buffers(cs);
    return 1;
on_error:
    err_report(res);
    reset_curl_buffers(cs);
    return 0;
}

int ac_alfaProSetup(t_at_rtsp_session* sess, int media_type) {
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    char transport[AC_RTSP_TRANSPORT_SIZE];

    if(ag_isCamInterleavedMode())
        make_il_transport_string(transport, sizeof(transport), media_type);
    else
        make_rt_transport_string(transport, sizeof(transport), (media_type == AC_RTSP_VIDEO_SETUP)?sess->media.rt_media.video.dst.port.rtp:sess->media.rt_media.audio.dst.port.rtp);

    if(media_type == AC_RTSP_VIDEO_SETUP) {
        if (res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, sess->video_url), res != CURLE_OK) goto on_error;
    }
    else {
        if (res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, sess->audio_url), res != CURLE_OK) goto on_error;
    }
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_TRANSPORT, transport), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(cs->h);

    if(ac_http_analyze_perform(res, cs->h, __FUNCTION__) != CURLE_OK) goto on_error;

    pu_log(LL_INFO, "%s: Header = \n%s", __FUNCTION__, cs->header_buf.buf);

    if(!ag_isCamInterleavedMode()) {
        int port = get_media_port(cs->header_buf.buf);
        if (!port) {
            pu_log(LL_ERROR, "%s: Can not get media port from camera transport string", __FUNCTION__);
            return 0;
        }

        char lip[20] = {0};
        get_source_ip(lip, sizeof(lip), cs->header_buf.buf);

        char *ipd = au_strdup(strlen(lip) ? lip : ag_getCamIP());
        if (!ipd) {
             pu_log(LL_ERROR, "%s: Memory allocation error ar %d", __FUNCTION__, __LINE__);
            return 0;
        }
        if (media_type == AC_RTSP_VIDEO_SETUP) {
            sess->media.rt_media.video.src.port.rtp = port;
            sess->media.rt_media.video.src.port.rtcp = port + 1;
            sess->media.rt_media.video.src.ip = ipd;
        } else {  /* AC_ALFA_AUDIO_SETUP */
            sess->media.rt_media.audio.src.port.rtp = port;
            sess->media.rt_media.audio.src.port.rtcp = port + 1;
            sess->media.rt_media.audio.src.ip = ipd;
        }
    }
    reset_curl_buffers(cs);
    return 1;
on_error:
    err_report(res);
    reset_curl_buffers(cs);
    return 0;
}

int ac_alfaProPlay(t_at_rtsp_session* sess) {
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    pu_log(LL_INFO, "%s: start", __FUNCTION__);
    AT_DT_RT(sess->device, AC_CAMERA, 0);

    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_STREAM_URI, sess->url), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(cs->h, CURLOPT_RANGE, AC_PLAY_RANGE), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(cs->h);

    if(ac_http_analyze_perform(res, cs->h,  __FUNCTION__) != CURLE_OK) goto on_error;

    curl_easy_setopt(cs->h, CURLOPT_RANGE, NULL);

    pu_log(LL_INFO, "%s: finished OK", __FUNCTION__);
    reset_curl_buffers(cs);
    return 1;
on_error:
    err_report(res);
    reset_curl_buffers(cs);
    return 0;
}

int ac_alfaProTeardown(t_at_rtsp_session* sess) {
    pu_log(LL_DEBUG, "%s starts", __FUNCTION__);
    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    AT_DT_RT(sess->device, AC_CAMERA, 0);

    if (res = curl_easy_setopt(cs->h, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN), res != CURLE_OK) return err_report(res);

    reset_curl_buffers(cs);
    res = curl_easy_perform(cs->h);


    if(ac_http_analyze_perform(res, cs->h, __FUNCTION__) != CURLE_OK) return err_report(res);

    pu_log(LL_INFO, "%s: Finished OK", __FUNCTION__);
    reset_curl_buffers(cs);
    return 1;
}

int getAlfaProConnSocket(t_at_rtsp_session* sess) {
    AT_DT_RT(sess->device, AC_CAMERA, -1);

    CURLcode res = CURLE_OK;
    t_curl_session* cs = sess->session;

    curl_socket_t sockfd;
/* Extract the socket from the curl handle */
    if(res = curl_easy_getinfo(cs->h, CURLINFO_ACTIVESOCKET, &sockfd), res != CURLE_OK) {
        err_report(res);
        return -1;
    }
    return sockfd;
}