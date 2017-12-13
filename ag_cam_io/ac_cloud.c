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
*/

#include <string.h>
#include <ctype.h>

#include "pu_logger.h"
#include "lib_http.h"

#include "ac_http.h"
#include "ag_settings.h"

#include "ac_cloud.h"

#define AC_HTTP_MAIN_STREAMING_INTERFACE1   "/cloud/json/settingsServer/streaming?"
#define AC_HTTP_MAIN_STREAMING_POSTFIX1     "&connected=false"
#define AC_HTTP_CLOUD_AUTH_PREFIX           "PPCAuthorization: esp token="
#define AC_HTTP_MAIN_STREAMING_INTERFACE2   "/session?deviceId="
#define AC_HTTP_MAIN_STREAMING_INTERFACE3   "/cloud/json/settingsServer?type=streaming&deviceId="

#define AC_HTTP_HTTP    "http://"
#define AC_HTTP_HTTPS   "https://"

static const char* F_HOST = "host";
static const char* F_PATH = "path";
static const char* F_PORT = "port";
static const char* F_SSL = "ssl";

static const char* F_RES_CODE = "resultCode";
static const char* F_VS_SSL = "videoServerSsl";
static const char* F_VS_HOST = "videoServer";
static const char* F_SESS_ID = "sessionId";


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

static int get_cloud_settings(const char* conn, const char* auth, char* answer, size_t size) {

    t_ac_http_handler* h = ac_http_prepare_get_conn(conn, auth);
    if(!h) return 0;

    int ret = 0;
    int rpt = AC_HTTP_REPEATS;
    while(rpt) {
        int rc = ac_perform_get_conn(h, answer, size);
        switch(rc) {
            case -1:        /* Retry */
                pu_log(LL_WARNING, "%s: Retry: attempt #%d", __FUNCTION__, rpt);
                rpt--;
                ret = 0;
                sleep(1);
                break;
            case 0:         /* Error */
                ret = 0;
                break;
             case 1:        /* Success */
                ret = 1;
                break;
             default:
                 pu_log(LL_ERROR, "%s: Unsupported RC = %d from ac_perform_get_conn()", __FUNCTION__, rc);
                ret = 0;
                break;
        }
     }
    ac_http_close_conn(h);
    return ret;
}

static int parse_url_head(const char* url, char* head, size_t h_size, char* rest, size_t r_size) {
    int i;
    if(i = findNCSubstr(url, AC_HTTP_HTTPS), i == 0) {
        strncpy(head, AC_HTTP_HTTPS, h_size);
        strncpy(rest, url+sizeof(AC_HTTP_HTTPS), r_size);
        return 1;
    }
    if(i = findNCSubstr(url, AC_HTTP_HTTP), i == 0) {
        strncpy(head, AC_HTTP_HTTP, h_size);
        strncpy(rest, url+sizeof(AC_HTTP_HTTP), r_size);
        return 1;
    }
    if(i < 0) { /* No head on URL */
        head[0] = '\0';
        strncpy(rest, url, r_size);
        return 1;
    }
    pu_log(LL_ERROR, "%s: Error parsing url %s", __FUNCTION__, url);
    return 0;   /* some rabbish */
}
static const char* create_conn_string(char* str, size_t size, const int* ssl, const char* url, const char* port, const char* pref1, const char* pref2, const char* dev_id, const char* posfix) {
    if(!url) {
        pu_log(LL_ERROR, "%s: URL field is NULL. Exiting", __FUNCTION__);
        return NULL;
    }

    if(!ssl) strncpy(str, url, size);
    else {
        char head[10] = {0};
        char rest[LIB_HTTP_MAX_URL_SIZE] ={0};
        if(!parse_url_head(url, head, sizeof(head), rest, sizeof(rest))) return NULL;
        if(*ssl)
            strncpy(str, AC_HTTP_HTTPS, size);
        else
            strncpy(str, AC_HTTP_HTTP, size);
        strncat(str, rest, size - strlen(str)-1);
    }

    if(port) {
        strncat(str, ":", size - strlen(str)-1);
        strncat(str, port, size - strlen(str)-1);
    }

    if(pref1) strncat(str, pref1, size - strlen(str)-1);
    if(pref2) strncat(str, pref2, size - strlen(str)-1);
    if(dev_id) strncat(str, dev_id, size - strlen(str)-1);
    if(posfix) strncat(str, posfix, size - strlen(str)-1);

    return str;
}

static const char* create_auth_string(char* str, size_t size, const char* auth_preffix, const char* auth) {
    if(!auth_preffix || !auth) {
        pu_log(LL_ERROR, "%s: One of input parameters is NULL. Exiting", __FUNCTION__);
        return NULL;
    }
    strncpy(str, auth_preffix, size);
    strncat(str, auth, size-strlen(str)-1);
    return str;
}

static int parse_cloud_settings(const char* answer, char* host, size_t h_size, char* port, size_t p_size, char* path, size_t pth_size, int* ssl) {
    cJSON* obj = NULL;
    cJSON* item;
    int ret = 0;

    if(obj = cJSON_Parse(answer), !obj) goto on_error;

    if(item = cJSON_GetObjectItem(obj, F_HOST), !item) goto on_error;
    strncpy(host, item->valuestring, h_size);

    if(item = cJSON_GetObjectItem(obj, F_PORT), !item) goto on_error;
    strncpy(port, item->valuestring, p_size);

    if(item = cJSON_GetObjectItem(obj, F_PATH), !item) goto on_error;
    strncpy(path, item->valuestring, pth_size);

    if(item = cJSON_GetObjectItem(obj, F_SSL), !item) goto on_error;
    *ssl = (item->type == cJSON_True)?1:0;

    ret = 1;

on_error:
    if(!ret) pu_log(LL_ERROR, "%s: Error parsing the loud answer %s", __FUNCTION__, answer);
    if(obj) cJSON_Delete(obj);
    return ret;
}

static int parse_video_settings(const char* answer, int* rc, int* ssl, char* host, size_t h_size, char* port, size_t p_size, char* sess, size_t s_size) {
    cJSON* obj = NULL;
    cJSON* item;
    int ret = 0;

    if(obj = cJSON_Parse(answer), !obj) goto on_error;

    if(item = cJSON_GetObjectItem(obj, F_RES_CODE), !item) goto on_error;
    *rc = item->valueint;

    if(item = cJSON_GetObjectItem(obj, F_VS_SSL), !item) goto on_error;
    *ssl = (item->type == cJSON_True)?1:0;

    if(item = cJSON_GetObjectItem(obj, F_PORT), !item) goto on_error;
    strncpy(host, item->string, h_size);
    int i = findNCSubstr(host, ":");
    if(i < 0) {
        pu_log(LL_ERROR, "s: port is not found in %s - %s", __FUNCTION__, F_VS_HOST, host);
        goto on_error;
    }
    else {
        host[i] = '\0';
        strncpy(port, host+i+1, p_size);
    }

    if(item = cJSON_GetObjectItem(obj, F_SESS_ID), !item) goto on_error;
    strncpy(sess, item->string, s_size);

    ret = 1;

    on_error:
    if(!ret) pu_log(LL_ERROR, "%s: Error parsing the loud answer %s", __FUNCTION__, answer);
    if(obj) cJSON_Delete(obj);
    return ret;
}

static int parst_ws_settings(const char* answer, char* w_host, size_t h_size, char* w_port, size_t p_size) {
    int i = findNCSubstr(answer, ":");
    if(i < 0) goto on_error;
    if(i = findNCSubstr(answer+i+1, ":"), i < 0) goto on_error;
    if(i > h_size) goto on_error;
    strncpy(w_host, answer, (size_t)i);
    w_host[i] = '\0';
    getStrNumber(w_port, p_size, answer+i+1);

    return 1;
on_error:
    pu_log(LL_ERROR, "%s: Port # is not found in Web Socket connection string '%s'.Exiting", __FUNCTION__, answer);
    return 0;
}

int ac_cloud_get_params(char* v_url, size_t v_size, int v_port, char* v_sess, size_t vs_size, char* w_url, size_t w_size, int w_port, char* w_sess, size_t ws_size) {
    int ret = 0;
    char answer[LIB_HTTP_MAX_MSG_SIZE] = {0};
    char conn[LIB_HTTP_MAX_MSG_SIZE] = {0};
    char auth[LIB_HTTP_MAX_MSG_SIZE] = {0};

    char host2[LIB_HTTP_MAX_URL_SIZE] = {0};
    char path2[LIB_HTTP_MAX_URL_SIZE] = {0};
    char port2[10] = {0};
    int ssl = 0;
    int rc = 0;

    if(!ac_http_init()) return ret;

    if(!create_conn_string(conn, sizeof(conn), NULL, ag_getMainURL(), NULL, AC_HTTP_MAIN_STREAMING_INTERFACE1, NULL, ag_getProxyID(), AC_HTTP_MAIN_STREAMING_POSTFIX1)) goto on_error;
    if(!create_auth_string(auth, sizeof(auth), AC_HTTP_CLOUD_AUTH_PREFIX,  ag_getProxyAuthToken())) goto on_error;

    if(!get_cloud_settings(conn, auth, answer, sizeof(answer))) goto on_error;
    if(!parse_cloud_settings(answer, host2, sizeof(host2), port2, sizeof(port2), path2, sizeof(path2),&ssl)) goto on_error;

    if(!create_conn_string(conn, sizeof(conn), &ssl, host2, port2, path2, AC_HTTP_MAIN_STREAMING_INTERFACE2, ag_getProxyID(), NULL)) goto on_error;

    if(!get_cloud_settings(conn, auth, answer, sizeof(answer))) goto on_error;
    if(!parse_video_settings(answer, &rc, &ssl, v_url, v_size, port2, sizeof(port2), v_sess, vs_size)) goto on_error;
    strncpy(w_sess, v_sess, ws_size-1);
    v_port = atoi(port2);

    if(!create_conn_string(conn, sizeof(conn), &ssl, ag_getMainURL(), NULL, AC_HTTP_MAIN_STREAMING_INTERFACE3, NULL, ag_getProxyID(), NULL)) goto on_error;
    if(!get_cloud_settings(conn, NULL, answer, sizeof(answer))) goto on_error;
    if(!parst_ws_settings(answer, w_url, w_size, port2, sizeof(port2))) goto on_error;
    w_port = atoi(port2);

    ret = 1;

on_error:
    ac_http_close();
    return ret;
}
