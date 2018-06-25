/*
 *  Copyright 2018 People Power Company
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
 Created by gsg on 12/06/18.
*/
#include <strings.h>
#include <curl/curl.h>

#include "lib_http.h"
#include "pu_logger.h"

#include "oobe_config.h"
#include "OOBE_defaults.h"

#include "oobe_connectivity.h"

enum {OH_ERROR, OH_OK, OH_CONTINUE};

static

static int http_test_connection(char* diagn, size_t s_diagn) {
    return OH_ERROR;
}

static int request_for_registration(const char* url, const char* device_id, const char* activation_key, char* answer, size_t size) {
    int rc = 1;

    CURL *curl = NULL;
    CURLcode res = CURLE_OK;
    struct curl_slist* slist = NULL;
    char full_request[LIB_HTTP_MAX_URL_SIZE]={0};
    char acyivation_key_hdr[128] = {0};
    char reply_buf[LIB_HTTP_MAX_MSG_SIZE] = {0};
    char* reply;



/* Now lets create the head and run it */
    if(curl=curl_easy_init(), !curl) {
        pu_log(LL_ERROR, "%s: curl_easy_init fails", __FUNCTION__);
        goto on_error;
    }

/* Header data */
    if(slist = curl_slist_append(slist, OOBE_HD_CONTENT_TYPE), !slist) {
        pu_log(LL_ERROR, "%s: curl_slist_append fails on %s", __FUNCTION__, OOBE_HD_CONTENT_TYPE);
        goto on_error;
    }
    snprintf(acyivation_key_hdr, sizeof(acyivation_key_hdr)-1, OOBE_HD_ACTIVATION_KEY_FMT, activation_key);
    if(slist = curl_slist_append(slist, acyivation_key_hdr), !slist) {
        pu_log(LL_ERROR, "%s: curl_slist_append fails on %s", __FUNCTION__, acyivation_key_hdr);
        goto on_error;
    }
    if(res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist), res != CURLE_OK) goto on_error;

/* Prepare full POST body: .../cloud/json/devices?deviceId=<device_id>&deviceType=7000&authToken=true */
    snprintf(full_request, sizeof(full_request)-1, OOBE_REGISTRATOIN_FMT, url, device_id);
    pu_log(LL_DEBUG, "%s: POST request: %s", __FUNCTION__, full_request);

    if(res = curl_easy_setopt(curl, CURLOPT_URL, full_request), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, LIB_HTTP_DEFAULT_CONNECT_TIMEOUT_SEC), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_POST, 1L), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, 0L), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_TIMEOUT, LIB_HTTP_DEFAULT_TRANSFER_TIMEOUT_SEC), res != CURLE_OK) goto on_error;

/* Run the request */
    while(1) {
        bzero(reply_buf, sizeof(reply_buf));
        reply = reply_buf;
        if(res = curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback), res != CURLE_OK) goto on_error;
        if(res = curl_easy_setopt(curl, CURLOPT_READDATA, &reply), res != CURLE_OK) goto on_error;

        res = curl_easy_perform(curl);
    }

    goto on_exit:

on_error:
    rc = 0;
on_exit:
    if(res != CURLE_OK) pu_log(LL_ERROR, "%s: %s", __FUNCTION__, curl_easy_strerror(res));
    if(slist) curl_slist_free_all(slist);
    if(curl) curl_easy_cleanup(curl);
    return 0;
}
static int parse_answer(const char* answer, char* auth, size_t s_auth, char* host, size_t s_host, char* uri, size_t s_uri, char* port, size_t s_port, int use_ssl) {
    return 0;
}

int oobe_registerDevice() {
    char answer[LIB_HTTP_MAX_MSG_SIZE] = {0};
    char auth[LIB_HTTP_AUTHENTICATION_STRING_SIZE] = {0};
    char host[LIB_HTTP_MAX_URL_SIZE] = {0};
    char uri[LIB_HTTP_MAX_URL_SIZE] = {0};
    char port[10] = {0};
    int use_ssl = 1;

    int rc = request_for_registration(oobe_getMainCloudURL(), oobe_getProxyDeviceID(), oobe_getActivationKey(), answer, sizeof(answer));
    if(!rc) return 0;

    rc = parse_answer(answer, auth, sizeof(auth), host, sizeof(host), uri, sizeof(uri), port, sizeof(port), &use_ssl);
    if(!rc) return 0;

    if(!oobe_saveAuthToken(auth)) return 0;
    oobe_save_contact_url(host, uri, port, use_ssl);

    return 1;
}
int oobe_test_connection() {
    char diagnostics[1024] = {0};
    while(1) {
        switch(http_test_connection(diagnostics, sizeof(diagnostics))) {
            case OH_ERROR:
                pu_log(LL_ERROR, "%s: %s", __FUNCTION__, diagnostics);
                return 0;
            case OH_OK:
                return 1;
            case OH_CONTINUE:
                sleep(OOBE_HTTP_RETRIES_TO_SEC)
            default:
                break;
        }
    }
    return 1;
}
