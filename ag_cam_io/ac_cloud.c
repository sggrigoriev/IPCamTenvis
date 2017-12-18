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
#include <au_string/au_string.h>

#include "pu_logger.h"
#include "lib_http.h"

#include "au_string.h"
#include "ac_http.h"
#include "ag_settings.h"

#include "ac_cloud.h"

#define AC_HTTP_MAIN_STREAMING_INTERFACE1   "/cloud/json/settingsServer/streaming?deviceId="
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

static const char* F_SERVER = "server";
static const char* F_RES_CODE = "resultCode";
static const char* F_VS_SSL = "videoServerSsl";
static const char* F_VS_HOST = "videoServer";
static const char* F_SESS_ID = "sessionId";

static const char* F_WS_HEAD = "ws://";

static int get_cloud_settings(const char* conn, const char* auth, char* answer, size_t size) {

    pu_log(LL_DEBUG, "%s: URL = '%s', auth = '%s'", __FUNCTION__, conn, (auth)?auth:"N/A");

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
                rpt = 0;
                break;
             case 1:        /* Success */
                ret = 1;
                rpt = 0;
                break;
             default:
                 pu_log(LL_ERROR, "%s: Unsupported RC = %d from ac_perform_get_conn()", __FUNCTION__, rc);
                ret = 0;
                rpt = 0;
                break;
        }
     }
    ac_http_close_conn(h);
    pu_log(LL_DEBUG, "%s: Answer = '%s'", __FUNCTION__, answer);
    return ret;
}

static int parse_url_head(const char* url, char* head, size_t h_size, char* rest, size_t r_size) {
    int i;
    if(i = au_findSubstr(url, AC_HTTP_HTTPS, AU_NOCASE), i == 0) {
        if(!au_strcpy(head, AC_HTTP_HTTPS, h_size)) return 0;
        if(!au_strcpy(rest, url+strlen(AC_HTTP_HTTPS), r_size)) return 0;
        return 1;
    }
    if(i = au_findSubstr(url, AC_HTTP_HTTP, AU_NOCASE), i == 0) {
        if(!au_strcpy(head, AC_HTTP_HTTP, h_size)) return 0;
        if(!au_strcpy(rest, url+strlen(AC_HTTP_HTTP), r_size)) return 0;
        return 1;
    }
    if(i < 0) { /* No head on URL */
        head[0] = '\0';
        if(!au_strcpy(rest, url, r_size)) return 0;
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
    if(!str) {
        pu_log(LL_ERROR, "%s: out buffer is NULL. Exiting", __FUNCTION__);
        return NULL;
    }

    if(!ssl) {
        if(!au_strcpy(str, url, size)) return NULL;
    }
    else {
        char head[10] = {0};
        char rest[LIB_HTTP_MAX_URL_SIZE] ={0};
        if(!parse_url_head(url, head, sizeof(head), rest, sizeof(rest))) return NULL;
        if(*ssl) {
            if(!au_strcpy(str, AC_HTTP_HTTPS, size)) return NULL;
        }
        else {
            if(!au_strcpy(str, AC_HTTP_HTTP, size)) return NULL;
        }
        if(!au_strcat(str, rest, size)) return NULL;
    }

    if(port) {
        if(!au_strcat(str, ":", size)) return NULL;
        if(!au_strcat(str, port, size)) return NULL;
    }

    if(pref1) if(!au_strcat(str, pref1, size)) return NULL;
    if(pref2) if(!au_strcat(str, pref2, size)) return NULL;
    if(dev_id) if(!au_strcat(str, dev_id, size)) return NULL;
    if(posfix) if(!au_strcat(str, posfix, size)) return NULL;

    return str;
}

static const char* create_auth_string(char* str, size_t size, const char* auth_preffix, const char* auth) {
    if(!auth_preffix || !auth) {
        pu_log(LL_ERROR, "%s: One of input parameters is NULL. Exiting", __FUNCTION__);
        return NULL;
    }
    if(!au_strcpy(str, auth_preffix, size)) return NULL;
    if(!au_strcat(str, auth, size)) return NULL;
    return str;
}

/*{"resultCode":0,"server":{"type":"streaming","host":"sbox1.presencepro.com","path":"/streaming","port":8443,"ssl":true,"altPort":8080,"altSsl":false}}*/
static int parse_cloud_settings(const char* answer, char* host, size_t h_size, char* port, size_t p_size, char* path, size_t pth_size, int* ssl) {
    cJSON* obj = NULL;
    cJSON* map = NULL;
    cJSON* item;
    int ret = 0;

    if(obj = cJSON_Parse(answer), !obj) goto on_error;

    if(map = cJSON_GetObjectItem(obj, F_SERVER), !map) goto on_error;

    if(item = cJSON_GetObjectItem(map, F_HOST), !item) goto on_error;
    if(!au_strcpy(host, item->valuestring, h_size)) goto on_error;
    host[h_size-1] = '\0';

    if(item = cJSON_GetObjectItem(map, F_PORT), !item) goto on_error;
    snprintf(port, p_size-1, "%d", item->valueint);
    port[p_size-1] = '\0';

    if(item = cJSON_GetObjectItem(map, F_PATH), !item) goto on_error;
    if(!au_strcpy(path, item->valuestring, pth_size)) goto on_error;
    path[pth_size-1] = '\0';

    if(item = cJSON_GetObjectItem(map, F_SSL), !item) goto on_error;
    *ssl = (item->type == cJSON_True)?1:0;

    ret = 1;

on_error:
    if(!ret) pu_log(LL_ERROR, "%s: Error parsing the cloud answer %s", __FUNCTION__, answer);
    if(obj) cJSON_Delete(obj);
    return ret;
}

/* {"resultCode":0,"apiServerSsl":false,"videoServer":"stream.presencepro.com:443","videoServerSsl":true,"sessionId":"2b1jnQsKKDaYWuGSmD3HzOnXu"} */
static int parse_video_settings(const char* answer, int* rc, int* ssl, char* host, size_t h_size, char* port, size_t p_size, char* sess, size_t s_size) {
    cJSON* obj = NULL;
    cJSON* item;
    int ret = 0;

    if(obj = cJSON_Parse(answer), !obj) goto on_error;

    if(item = cJSON_GetObjectItem(obj, F_RES_CODE), !item) goto on_error;
    *rc = item->valueint;

    if(item = cJSON_GetObjectItem(obj, F_VS_SSL), !item) goto on_error;
    *ssl = (item->type == cJSON_True)?1:0;

    if(item = cJSON_GetObjectItem(obj, F_VS_HOST), !item) goto on_error;
    if(!au_strcpy(host, item->valuestring, h_size)) goto on_error;
    int i = au_findSubstr(host, ":", AU_CASE);
    if(i < 0) {
        pu_log(LL_ERROR, "%s: video server host is not found in %s - %s", __FUNCTION__, F_VS_HOST, host);
        goto on_error;
    }
    else {
        host[i] = '\0';
        if(!au_strcpy(port, host+i+1, p_size)) goto on_error;
    }

    if(item = cJSON_GetObjectItem(obj, F_SESS_ID), !item) goto on_error;
    if(!au_strcpy(sess, item->valuestring, s_size)) goto on_error;

    ret = 1;

    on_error:
    if(!ret) pu_log(LL_ERROR, "%s: Error parsing video settings %s", __FUNCTION__, answer);
    if(obj) cJSON_Delete(obj);
    return ret;
}

/* ws://sbox1.presencepro.com:8080 */
static int parse_ws_settings(const char* answer, char* w_host, size_t h_size, char* w_port, size_t p_size) {
    int pos;
    if(!au_getSection(w_host, h_size, answer, F_WS_HEAD, ":", AU_NOCASE)) goto on_error;

    if(pos = au_findSubstr(answer+strlen(w_host)+strlen(F_WS_HEAD), ":", AU_NOCASE), pos < 0) goto on_error;
    au_getNumber(w_port, p_size, answer+pos + strlen(w_host)+strlen(F_WS_HEAD));

    return 1;
on_error:
    pu_log(LL_ERROR, "%s: Port # is not found in Web Socket connection string '%s'.Exiting", __FUNCTION__, answer);
    return 0;
}

int ac_cloud_get_params(char* v_url, size_t v_size, int* v_port, char* v_sess, size_t vs_size, char* w_url, size_t w_size, int* w_port, char* w_sess, size_t ws_size) {
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
    if(!au_strcpy(w_sess, v_sess, ws_size)) goto on_error;

    *v_port = atoi(port2);

    if(!create_conn_string(conn, sizeof(conn), &ssl, ag_getMainURL(), NULL, AC_HTTP_MAIN_STREAMING_INTERFACE3, NULL, ag_getProxyID(), NULL)) goto on_error;
    if(!get_cloud_settings(conn, NULL, answer, sizeof(answer))) goto on_error;
    if(!parse_ws_settings(answer, w_url, w_size, port2, sizeof(port2))) goto on_error;

    *w_port = atoi(port2);

    ret = 1;

on_error:
    ac_http_close();
    return ret;
}
