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
 Created by gsg on 12/12/17.
 Here we'll put al common curl parts
*/
#include <string.h>
#include <errno.h>

#include "lib_http.h"
#include "pu_logger.h"

#include "au_string.h"
#include "ag_defaults.h"
#include "ag_settings.h"

#include "ac_http.h"


static size_t writer(void *ptr, size_t size, size_t nmemb, void *userp) {
    t_ac_callback_buf *dataToRead = (t_ac_callback_buf *) userp;
    char *data = (char *)ptr;
    if (dataToRead == NULL || dataToRead->buf == NULL) {
        pu_log(LL_ERROR, "writer: dataToRead == NULL");
        return 0;
    }

    /* keeping one byte for the null byte */
    if((strlen(dataToRead->buf)+(size * nmemb)) > (dataToRead->sz - 1))
    {
#if __WORDSIZE == 64
        pu_log(LL_WARNING, "%s: buffer overflow would result -> strlen(writeData): %lu, (size * nmemb): %lu, max size: %u",
#else
                pu_log(LL_WARNING, "writer: buffer overflow would result -> strlen(writeData): %u, (size * nmemb): %u, max size: %u",
#endif
               __FUNCTION__, strlen(dataToRead->buf), (size * nmemb), dataToRead->sz);
        return 0;
    }
    strncat(dataToRead->buf, data, (size * nmemb));
    return (size * nmemb);
}

int ac_http_init() {
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        pu_log(LL_ERROR, "%s: Error on cUrl initialiation. Exiting.", __FUNCTION__);
        return 0;
    }
    return 1;
}

void ac_http_close() {
    curl_global_cleanup();
}

t_ac_http_handler* ac_http_prepare_get_conn(const char* url_string, const char* auth_string) {
    CURLcode curlResult = CURLE_OK;

    t_ac_http_handler* h = NULL;

    if(h = calloc(sizeof(t_ac_http_handler), 1), !h) {
        pu_log(LL_ERROR, "%s: Memory allocation error", __FUNCTION__);
        goto out;
    }
    if(h->wr_buf.buf = calloc(LIB_HTTP_MAX_MSG_SIZE, 1), !h->wr_buf.buf) {
        pu_log(LL_ERROR, "%s: Memory allocation error", __FUNCTION__);
        goto out;
    }
    h->wr_buf.sz = LIB_HTTP_MAX_MSG_SIZE;

    if(h->h = curl_easy_init(), !h->h) {
        pu_log(LL_ERROR, "%s: Error on get cURL handler", __FUNCTION__);
        goto out;
    }

    if(curlResult = curl_easy_setopt(h->h, CURLOPT_URL, url_string), curlResult != CURLE_OK) goto out;
    if(auth_string) {  /* Have to add auth token */
        h->slist = curl_slist_append(h->slist, auth_string);
        if(curlResult = curl_easy_setopt(h->h, CURLOPT_HTTPHEADER, h->slist), curlResult != CURLE_OK) goto out;
    }

    if(strlen(ag_getCurloptCAPath())) {
        if(curlResult = curl_easy_setopt(h->h, CURLOPT_CAPATH, ag_getCurloptCAPath()), curlResult != CURLE_OK) goto out;
    }
    if(curlResult = curl_easy_setopt(h->h, CURLOPT_SSL_VERIFYPEER, (long)ag_getCurloptSSLVerifyer()), curlResult != CURLE_OK) goto out;


    if(curlResult = curl_easy_setopt(h->h, CURLOPT_ERRORBUFFER, h->err_buf), curlResult != CURLE_OK) goto out;
    if(curlResult = curl_easy_setopt(h->h, CURLOPT_WRITEFUNCTION, writer), curlResult != CURLE_OK) goto out;
    if(curlResult = curl_easy_setopt(h->h, CURLOPT_WRITEDATA, &h->wr_buf), curlResult != CURLE_OK) goto out;
    if(curlResult = curl_easy_setopt(h->h, CURLOPT_BUFFERSIZE, sizeof(h->wr_buf.buf)), curlResult != CURLE_OK) goto out;

out:
    if(curlResult != CURLE_OK) {
        pu_log(LL_ERROR, "%s: %s", __FUNCTION__, curl_easy_strerror(curlResult));
        ac_http_close_conn(h);
        return NULL;
    }
    return h;
}

/* -1 - retry, 0 - error, 1 - OK */
int ac_perform_get_conn(t_ac_http_handler* h, char* answer, size_t size) {
    CURLcode curlResult = CURLE_OK;
    long curlErrno = 0;
    int ret;

    if(!h) return 0;
    if(!h->h) return 0;
    if(!answer) {
        pu_log(LL_ERROR, "%s: output buffer is NULL. Exiting", __FUNCTION__);
        return 0;
    }
    answer[0] = '\0';

    curlResult = curl_easy_perform(h->h);

    if(ac_http_analyze_perform(curlResult, h->h, __FUNCTION__) != CURLE_OK) goto out;

    /* the following is a special case - a time-out from the server is going to return a */
    /* string with 1 character in it ... */
    if (strlen(h->wr_buf.buf) > 1) {
        /* put the result into the main buffer and return */
        pu_log(LL_DEBUG, "%s: received msg length %d", __FUNCTION__, strlen(h->wr_buf.buf));
        if(!au_strcpy(answer, h->wr_buf.buf, size)) goto out;
     }
    else {
        pu_log(LL_DEBUG, "%s: received time-out message from the server. RX len = %d", __FUNCTION__, strlen(h->wr_buf.buf));
        curlErrno = EAGAIN;
        goto out;
    }
out:
    h->wr_buf.buf[0] = '\0';    /* prepare rx_buf for new inputs */
    if ((curlErrno == EAGAIN) || (curlErrno == ETIMEDOUT)) ret = AC_HTTP_RC_RETRY;   /* timrout case */
    else if (!curlErrno) ret = AC_HTTP_RC_OK;   /* Got smth to read */
    else ret = AC_HTTP_RC_ERROR;
    return ret;
}

void ac_http_close_conn(t_ac_http_handler* h) {
    if(h) {
        if (h->slist) curl_slist_free_all(h->slist);
        if (h->h) (curl_easy_cleanup(h->h), h->h = NULL);
        if (h->wr_buf.buf) free(h->wr_buf.buf);
        free(h);
        h = NULL;
    }
}

long ac_http_analyze_perform(CURLcode perform_rc, CURLSH* handler, const char* function) {
    long httpResponseCode = 0;
    long httpConnectCode = 0;
    long curlErrno = 0;

    curl_easy_getinfo(handler, CURLINFO_RESPONSE_CODE, &httpResponseCode );
    curl_easy_getinfo(handler, CURLINFO_HTTP_CONNECTCODE, &httpConnectCode );

    if (httpResponseCode >= 300 || httpConnectCode >= 300) {
        pu_log(LL_ERROR, "%s: HTTP error response code at %s:%ld, connect code:%ld", __FUNCTION__, function, httpResponseCode, httpConnectCode);
        if((httpResponseCode == AC_HTTP_UNAUTH) && (httpConnectCode == 0L)) curlErrno = AC_HTTP_UNAUTH;
    }
    else if (perform_rc != CURLE_OK) {
        if (perform_rc == CURLE_ABORTED_BY_CALLBACK) {
            curlErrno = EAGAIN;
            pu_log(LL_DEBUG, "%s: quitting curl transfer at %s: %d %s", __FUNCTION__, function, curlErrno, strerror((int)curlErrno));
        }
        else {
            if (curl_easy_getinfo(handler, CURLINFO_OS_ERRNO, &curlErrno) != CURLE_OK) {
                curlErrno = ENOEXEC;
                pu_log(LL_ERROR, "%s: curl_easy_getinfo returned CURLINFO_OS_ERRNO at %s", __FUNCTION__, function);
            }
            if (perform_rc == CURLE_OPERATION_TIMEDOUT) curlErrno = ETIMEDOUT; /* time out error must be distinctive */
            else if (curlErrno == 0) curlErrno = ENOEXEC; /* can't be equalt to 0 if curlResult != CURLE_OK */

            pu_log(LL_WARNING, "%s at %s: %s, %s", __FUNCTION__, function, curl_easy_strerror(perform_rc), strerror((int) curlErrno));
        }
    }
    return curlErrno;
}