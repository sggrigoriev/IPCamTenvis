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
 Created by gsg on 18/10/18.
*/



#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <curl/curl.h>

#include "lib_timer.h"
#include "cJSON.h"
#include "pu_logger.h"

#include "ag_defaults.h"
#include "aq_queues.h"
#include "ac_cam.h"
#include "ao_cmd_cloud.h"
#include "ao_cmd_proxy.h"
#include "ag_settings.h"

#include "at_cam_files_sender.h"

#define PT_THREAD_NAME "FILES_SENDER"

static volatile int is_stop = 0;
static pthread_t id;
static pthread_attr_t attr;

static pu_queue_t* from_main;
static pu_queue_t* to_proxy;
static pu_queue_msg_t q_msg[1024];    /* Buffer for messages received */

typedef struct {
    cJSON* root;
    cJSON* arr;
    char ft;
    int idx;
    int total;
} fld_t;

typedef struct {
    const char* name;
    const char* ext;
    char type;
    size_t size;
} fd_t;

/******************************************************************/
/*         Send files functions                                   */
typedef struct {
    char url[512];
    const char* device_id;
    const char* auth_token;
    const char* ext;
    unsigned long f_size;
    int f_type;             /* 0 - any, 1- video, 2 - image, 3 - audio */
} sf_url_in_t;
/* TODO Make inline all this char <-> char* staff*/
static void fill_sf_url_in(const fd_t fd, sf_url_in_t* ip) {
    snprintf(ip->url, sizeof(ip->url), "%s/%s", ag_getMainURL(), DEFAULT_FILES_UPL_PATH);
    ip->device_id = ag_getProxyID();
    ip->auth_token = ag_getProxyAuthToken();
    ip->ext = fd.ext;
    ip->f_size = fd.size;
    switch(fd.type) {
        case 'M':
            ip->f_type = 1;
            break;
        case 'S':
            ip->f_type = 3;
            break;
        case 'P':
            ip->f_type = 2;
            break;
        default:
            ip->f_type = 0;
            pu_log(LL_ERROR, "%s: Unrecognized file type %c", __FUNCTION__, fd.type);
            break;
    }
}

static CURL* init(){
    CURL *curl;
    if(curl = curl_easy_init(), !curl) {
        pu_log(LL_ERROR, "%s: Error on curl_easy_init call.", __FUNCTION__);
        return 0;
    }
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 120L);   /* timeout if the thansmission slower than 1 byte per 2 minutes */
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 30L);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
/*    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, 0L); */
/* CA_INFO & SSL_VERIFYER */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ag_getCurloptSSLVerifyPeer());
    if(strlen(ag_getCurloptCAInfo())) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, ag_getCurloptCAInfo());
    }
    return curl;
}

static struct curl_slist* make_getSF_header(const sf_url_in_t* in_par, struct curl_slist* sl) {
    char buf[512]={0};
    const char* content;
    switch (in_par->f_type) {
        case 1:
            content = "video/";
            break;
        case 2:
            content = "image/";
            break;
        case 3:
            content = "audio/";
            break;
         default:
            pu_log(LL_ERROR, "%s: Unknown content type %d", __FUNCTION__, in_par->f_type);
            return NULL;
            break;
    }
    snprintf(buf, sizeof(buf)-1, "Content-Type: %s%s", content, in_par->ext);
    if(sl = curl_slist_append(sl, buf), !sl) return NULL;

    snprintf(buf, sizeof(buf)-1, "PPCAuthorization: esp token=%s", in_par->auth_token);
    sl = curl_slist_append(sl, buf);
    return sl;
}
/*
 * parse {"<hdr_name>":"<hdr_value",...} message and add it into sl
 */
static struct curl_slist* make_SF_header(const char* upl_hdrs, struct curl_slist* sl) {
    struct curl_slist* ret = NULL;

    cJSON* obj = cJSON_Parse(upl_hdrs);
    if(!obj) {
        pu_log(LL_ERROR, "%s: error parsing %s", __FUNCTION__, upl_hdrs);
        return NULL;
    }
    cJSON* child = obj->child;
    if(!child) {
        pu_log(LL_ERROR, "%s: no headers found in %s", __FUNCTION__, upl_hdrs);
        goto on_error;
    }
    while(child) {
        char buf[512]={0};
        snprintf(buf, sizeof(buf), "%s: %s", child->string, child->valuestring);
        sl = curl_slist_append(sl, buf);
        child = child->next;
    }
    ret = sl;
    on_error:
    cJSON_Delete(obj);
    return ret;
}
static struct curl_slist* make_SFupdate_header(struct curl_slist* sl, const char* auth_token) {
    char buf[100] = {0};

    sl = curl_slist_append(sl, "Content-Type: application/json");
    snprintf(buf, sizeof(buf) - 1, "PPCAuthorization: esp token=%s", auth_token);
    sl = curl_slist_append(sl, buf);

    return sl;
}

static const char* make_getSF_url(const sf_url_in_t* in_par, char *url, size_t size) {
    snprintf(url, size-1,
             "%s?proxyId=%s&deviceId=%s&ext=%s&expectedSize=%lu&thumbnail=false&rotate=%d&incomplete=false&uploadUrl=true&type=%d",
             in_par->url,
             in_par->device_id,
             in_par->device_id,
             in_par->ext,
             in_par->f_size,
             (in_par->f_type==2)?180:0,       /* Turn on 180 if image */
             in_par->f_type
    );
    return url;
}
static const char* make_updSF_url(const sf_url_in_t* in_par, unsigned long fid, char* url, size_t size) {
    snprintf(url, size-1,
             "%s/%lu?proxyId=%s&incomplete=false",
             in_par->url,
             fid,
             in_par->device_id
    );
    return url;
}
/*
 * NB! answer should be freed after use!
 */
static char* post_n_reply(const struct curl_slist *hs, const char* url) {
    char* ret = NULL;
    char err_b[CURL_ERROR_SIZE]= {0};
    CURLcode res;
    FILE* fp = NULL;
    char* ptr = NULL;
    size_t sz=0;

    CURL* curl;
    if(curl = init(), !curl) return 0;

    if(fp = open_memstream(&ptr, &sz), !fp) {
        pu_log(LL_ERROR, "%s: Error open memstream: %d - %s", __FUNCTION__, errno, strerror(errno));
        goto on_error;
    }

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_b);
    if(res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_URL, url), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_POST, 1L), res != CURLE_OK) goto on_error;

    if(res = curl_easy_perform(curl), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Curl error %s", __FUNCTION__, curl_easy_strerror(res));
        goto on_error;
    }

    fflush(fp);
    if(!ptr) {
        pu_log(LL_ERROR, "%s: Cloud returns empty answer on %s request", __FUNCTION__, url);
        goto on_error;
    }
    if(ret = strdup(ptr), !ret) {
        pu_log(LL_ERROR, "%s: Not enough memory on %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: Answer = %s", __FUNCTION__, ret);
on_error:
    if(fp)fclose(fp);
    if(ptr)free(ptr);
    curl_easy_cleanup(curl);
    return ret;
}

static char* put_n_reply(const struct curl_slist *hs, const char* url) {
    char* ret = NULL;
    char err_b[CURL_ERROR_SIZE]= {0};
    CURLcode res;

    FILE* fpr = NULL;
    char* ptrr = NULL;
    size_t szr=0;

    FILE* fpw = NULL;
    char* ptrw = NULL;
    size_t szw=0;

    CURL* curl;
    if(curl = init(), !curl) return 0;

    if(fpr = open_memstream(&ptrr, &szr), !fpr) {
        pu_log(LL_ERROR, "%s: Error open memstream: %d - %s", __FUNCTION__, errno, strerror(errno));
        goto on_error;
    }
    if(fpw = open_memstream(&ptrw, &szw), !fpw) {
        pu_log(LL_ERROR, "%s: Error open memstream: %d - %s", __FUNCTION__, errno, strerror(errno));
        goto on_error;
    }

    if(res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_b), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_URL, url), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fpw), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_READDATA, fpr), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_PUT, 1L), res != CURLE_OK) goto on_error;

    if(res = curl_easy_perform(curl), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Curl error %s", __FUNCTION__, curl_easy_strerror(res));
        goto on_error;
    }

    fflush(fpw);
    if(!ptrw) {
        pu_log(LL_ERROR, "%s: Cloud returns empty answer on %s request", __FUNCTION__, url);
        goto on_error;
    }
    if(ret = strdup(ptrw), !ret) {
        pu_log(LL_ERROR, "%s: Not enough memory on %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: Answer = %s", __FUNCTION__, ret);
on_error:
    if(fpr)fclose(fpr);
    if(ptrr)free(ptrr);
    if(fpw)fclose(fpw);
    if(ptrw)free(ptrw);

    curl_easy_cleanup(curl);
    return ret;
}

static int parse_cloud_answer(const char* ptr, char* sf_url, size_t sf_size, char* upl_hdrs, size_t upl_size, unsigned long *fid) {
    int ret = 0;
    cJSON* obj = cJSON_Parse(ptr);
    if(!obj) {
        pu_log(LL_ERROR, "%s: error %s parsing", __FUNCTION__, ptr);
        return 0;
    }
    cJSON* url = cJSON_GetObjectItem(obj, "contentUrl");
    if(!url) {
        pu_log(LL_ERROR, "%s: \"contentUrl\" field not found", __FUNCTION__);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: content url = %s", __FUNCTION__, url->valuestring);
    strncpy(sf_url, url->valuestring, sf_size);
    sf_url[sf_size-1] = '\0';
    cJSON* hdrs = cJSON_GetObjectItem(obj, "uploadHeaders");
    if(!hdrs) {
        pu_log(LL_ERROR, "%s: \"uploadHeaders\" field not found", __FUNCTION__);
        goto on_error;
    }
    char *hd = cJSON_PrintUnformatted(hdrs);
    pu_log(LL_DEBUG, "%s: upload headers = %s", __FUNCTION__, hd);
    strncpy(upl_hdrs, hd, upl_size);
    upl_hdrs[upl_size-1] = '\0';
    free(hd);
    cJSON* file_id = cJSON_GetObjectItem(obj, "fileRef");
    if(!file_id) {
        pu_log(LL_ERROR, "%s: \"fileRef\" field not foind", __FUNCTION__);
        goto on_error;
    }
    *fid = (unsigned long)(file_id->valueint);
    pu_log(LL_DEBUG, "%s: fileId = %lu", __FUNCTION__, *fid);
    ret = 1;

    on_error:
    cJSON_Delete(obj);
    return ret;
}
static int is_cloud_answerOK(const char* answer) {
    int ret = 0;

    if(!strlen(answer)) {
        pu_log(LL_ERROR, "%s: Timeout from cloud received. Empty answer", __FUNCTION__);
        return 0;
    }
    cJSON* obj = cJSON_Parse(answer);
    if(!obj) {
        pu_log(LL_ERROR, "%s: Error parsing the %s", __FUNCTION__, answer);
        goto on_error;
    }
    cJSON* rc = cJSON_GetObjectItem(obj, "resultCode");
    if(!rc) {
        pu_log(LL_ERROR, "%s: \"resultCode\" no found.", __FUNCTION__);
        goto on_error;
    }
    if(rc->valueint) {
        pu_log(LL_ERROR, "%s: Cloud returns error answer: %s", __FUNCTION__, answer);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: Answer on update = %s", __FUNCTION__, answer);
    ret = 1;
    on_error:
    cJSON_Delete(obj);
    return ret;
}


static int getSF_URL(const sf_url_in_t* in_par, const struct curl_slist *hs, unsigned long *fid, char* sf_url, size_t sf_size, char* upl_hdrs, size_t upl_size) {
    int ret = 0;

    char url_cmd[1024]={0};
    make_getSF_url(in_par, url_cmd, sizeof(url_cmd));

    char* ptr = post_n_reply(hs, url_cmd);
    if(!ptr) goto on_error;

    if(!parse_cloud_answer(ptr, sf_url, sf_size, upl_hdrs, upl_size, fid)) {
        pu_log(LL_ERROR, "%s: Error parsing answer from cloud %s", __FUNCTION__, ptr);
        goto on_error;
    }
    ret = 1;
on_error:
    if(ptr)free(ptr);
    return ret;
}
/*
 * Return 1 if OK, 0 if error, 2 if file not found
 */
static int sendFile(const char* sf_url, const struct curl_slist *hs, const char* f_path, unsigned long f_size) {
    int ret = 0;
    char err_b[CURL_ERROR_SIZE]= {0};
    CURL *curl;
    CURLcode res;

    char* ptr = NULL;
    size_t sz=0;
    FILE* fp = NULL;

    if(curl = init(), !curl) return 0;

    FILE* fd = fopen(f_path, "rb"); /* open file to upload */
    if(!fd) {
        pu_log(LL_ERROR, "%s: Open %s file error %d-%s", __FUNCTION__, f_path, errno, strerror(errno));
        if(errno == ENOENT) ret = 2;   /* No such file. It was sent already */
        goto on_error;
    }

    if(fp = open_memstream(&ptr, &sz), !fp) {
        pu_log(LL_ERROR, "%s: Error open memstream: %d - %s", __FUNCTION__, errno, strerror(errno));
        goto on_error;
    }
    if(res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_b), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_URL, sf_url), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(curl, CURLOPT_READDATA, fd), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)f_size), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp), res != CURLE_OK) goto on_error;

    if(res = curl_easy_perform(curl), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Curl error %s", __FUNCTION__, curl_easy_strerror(res));
        goto on_error;
    }

    fflush(fp);
    if(!ptr) {
        pu_log(LL_ERROR, "%s: Cloud returns empty answer on %s request", __FUNCTION__, sf_url);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: Answer = %s", __FUNCTION__, ptr);

    ret = 1;
on_error:
    if(fd)fclose(fd);
    if(fp)fclose(fp);
    if(ptr) free(ptr);
    curl_easy_cleanup(curl);
    return ret;
}
static int sendSF_update(const sf_url_in_t* in_par, struct curl_slist *hs, unsigned long fid) {
    int ret = 0;

    char* ptr = NULL;
    char url_cmd[1024]={0};

    make_updSF_url(in_par, fid, url_cmd, sizeof(url_cmd));
    pu_log(LL_DEBUG, "%s: URL = %s", __FUNCTION__, url_cmd);

    if(ptr = put_n_reply(hs, url_cmd), !ptr) goto on_error;
    if(!is_cloud_answerOK(ptr)) goto on_error;

    ret = 1;
on_error:
    if(ptr)free(ptr);
    return ret;
}
/*****************************************************************/
/*         Thread functions                                      */

static void clean_snap_n_video() {
    char path[256]={0};
    pu_log(LL_INFO, "%s: Clean SNAPSHOTS", PT_THREAD_NAME);
    snprintf(path, sizeof(path)-1, "%s/%s", DEFAULT_DT_FILES_PATH, DEFAULT_SNAP_DIR);
    ac_cam_clean_dir(path);

    pu_log(LL_INFO, "%s: Clean VIDEOS", PT_THREAD_NAME);
    snprintf(path, sizeof(path)-1, "%s/%s", DEFAULT_DT_FILES_PATH, DEFAULT_VIDEO_DIR);
    ac_cam_clean_dir(path);
}

static fld_t* open_flist(const char* msg) {
    fld_t* ret = NULL;

    cJSON* obj = cJSON_Parse(msg);
    if(!obj) return NULL;
    cJSON* type = cJSON_GetObjectItem(obj, "type");
    if(!type || (type->type != cJSON_String)) goto err;
    cJSON* arr = cJSON_GetObjectItem(obj, "filesList");
    if(!arr || (arr->type != cJSON_Array)) goto err;

    ret = calloc(sizeof(fld_t), 1);
    if(!ret) {
        pu_log(LL_ERROR, "%s: not enough memory", __FUNCTION__);
        goto err;
    }
    ret->root = obj;
    ret->arr = arr;
    ret->idx = 0;
    ret->total = cJSON_GetArraySize(arr);
    ret->ft = type->valuestring[0];
    return ret;
err:
    cJSON_Delete(obj);
    return ret;
}
static int get_next_f(fld_t* fld, fd_t* fd) {
    if(fld->idx < fld->total) {
        fd->name = cJSON_GetArrayItem(fld->arr, fld->idx)->valuestring;
        fd->type = fld->ft;
        fd->ext = ac_cam_get_file_ext(fd->name);
        fd->size = ac_get_file_size(fd->name);
        fld->idx++;
        return 1;
    }
    return 0;
}
static void close_flist(fld_t* fld) {
    cJSON_Delete(fld->root);
/* The rest fields are pointerd to the root */
    free(fld);
}
/*
 * Sending file to cloud
 * Return 0 if error and fileID if Ok
 * Return -1 if no file
 *
 */
static const unsigned long send_file(fd_t fd) {
    unsigned long ret = 0;

    char sf_url[1024]={0};
    char upl_hdrs[1024]={0};
    unsigned long file_id;

    struct curl_slist *hs1=NULL;
    struct curl_slist *hs2=NULL;

    sf_url_in_t ip;
    fill_sf_url_in(fd, &ip);

    if(hs1 = make_getSF_header(&ip, hs1), !hs1) goto on_error;

    if(!getSF_URL(&ip, hs1, &file_id, sf_url, sizeof(sf_url)-1, upl_hdrs, sizeof(upl_hdrs)-1)) {
        pu_log(LL_ERROR, "%s: error getting URL, no upload", __FUNCTION__);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: URL = %s, UPL_HDRS = %s", __FUNCTION__, sf_url, upl_hdrs);

    hs1 = make_SF_header(upl_hdrs, hs1);    /* Add header to exisning for file upload */
    int rc = sendFile(sf_url, hs1, fd.name, ip.f_size);
    if(!rc) {
        pu_log(LL_ERROR, "%s: error file %s upload", __FUNCTION__, fd.name);
        goto on_error;
    }
    else if(rc == 2) { /* file not found */
        ret = -1;
        goto on_error;
    }

    pu_log(LL_DEBUG, "%s: file %s sent OK", "make_SF_header", fd.name);

    hs2 = make_SFupdate_header(hs2, ip.auth_token);
    if(!sendSF_update(&ip, hs2, file_id)) {
        pu_log(LL_ERROR, "%s: error sending completion update for file %s", __FUNCTION__, fd.name);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: file %s uploaded OK", __FUNCTION__, fd.name);

/* Delete file if sent */
    if(!unlink(fd.name)) {
        pu_log(LL_DEBUG, "%s: File %s deleted", __FUNCTION__, fd.name);
    }
    else {
        pu_log(LL_ERROR, "%s: error deletion %s: %d - %s", __FUNCTION__, fd.name, errno, strerror(errno));
    }

    ret = file_id;
on_error:
    if(hs2) curl_slist_free_all(hs2);
    if(hs1) curl_slist_free_all(hs1);
    return ret;
}
/*
 * return queue with all not sent files
 * In ultimate case all this crap will be deleted after DEFAULT_TO_FOR_DIR_CLEANUP timeout
 */
static ac_cam_resend_queue_t* resend(ac_cam_resend_queue_t* q) {
    if(!q) {
        pu_log(LL_ERROR, "%s: Internal error: Resend Queue is NULL!", __FUNCTION__);
        return q;
    }
    char buf[LIB_HTTP_MAX_MSG_SIZE]={0};
    char* txt;
    if(q->md_arr) {
        txt = cJSON_PrintUnformatted(q->md_arr);
        if(txt) {
            snprintf(buf, sizeof(buf)-1, "{\"name\": \"sendFiles\", \"type\": \"%s\", \"filesList\": %s}", DEFAULT_MD_FILE_POSTFIX, txt);
            pu_queue_push(from_main, buf, strlen(buf) + 1);
            pu_log(LL_DEBUG, "%s: SF resends MD files", __FUNCTION__, txt);
            free(txt);
        }
        cJSON_Delete(q->md_arr); q->md_arr = NULL;
    }
    if(q->sd_arr) {
        txt = cJSON_PrintUnformatted(q->sd_arr);
        if(txt) {
            snprintf(buf, sizeof(buf)-1, "{\"name\": \"sendFiles\", \"type\": \"%s\", \"filesList\": %s}", DEFAULT_SD_FILE_POSTFIX, txt);
            pu_queue_push(from_main, buf, strlen(buf) + 1);
            pu_log(LL_DEBUG, "%s: SF resends SD files", __FUNCTION__, txt);
            free(txt);
        }
        cJSON_Delete(q->sd_arr); q->sd_arr = NULL;
    }
    if(q->snap_arr) {
        txt = cJSON_PrintUnformatted(q->snap_arr);
        if(txt) {
            snprintf(buf, sizeof(buf)-1, "{\"name\": \"sendFiles\", \"type\": \"%s\", \"filesList\": %s}", DEFAULT_SNAP_FILE_POSTFIX, txt);
            pu_queue_push(from_main, buf, strlen(buf) + 1);
            pu_log(LL_DEBUG, "%s: SF resends SNAP files", __FUNCTION__, txt);
            free(txt);
        }
        cJSON_Delete(q->snap_arr); q->snap_arr = NULL;
    }
    return q;
}

static int alert_number = 1;
static const char* inc_alert_number(char* buf, size_t size) {
    snprintf(buf, size, "%d", alert_number++);
    return buf;
}
static void send_alert_to_proxy(char type, unsigned long fileRef) {
    t_ac_cam_events ev;
    switch (type) {
        case 'M':
            ev = AC_CAM_STOP_MD;
            break;
        case 'S':
            ev = AC_CAM_STOP_SD;
            break;
        default:
            /* Not our case - get out of here */
            return;
    }
    char a_buf[20]={0};
    const char* a_num = inc_alert_number(a_buf, sizeof(a_buf) - 1);

    char f_buf[20]={0};
    const char* f_num;
    if(fileRef) {
        snprintf(f_buf, sizeof(f_buf), "%lu", fileRef);
        f_num = f_buf;
    }
    else {
        f_num = NULL;
    }
    char buf[LIB_HTTP_MAX_MSG_SIZE];
    const char *msg = ao_cloud_msg(ag_getProxyID(), "153", ao_cloud_alerts(ag_getProxyID(), a_num, ev, f_num), NULL, NULL, buf, sizeof(buf));
    if(!msg) {
        pu_log(LL_ERROR, "%s: message to cloud exceeds max size %d. Ignored", __FUNCTION__, LIB_HTTP_MAX_MSG_SIZE);
        return;
    }
    pu_queue_push(to_proxy, msg, strlen(msg)+1);
}

static void* thread_function(void* params) {
    from_main = aq_get_gueue(AQ_ToSF);
    to_proxy = aq_get_gueue(AQ_ToProxyQueue);
    pu_queue_event_t events = pu_add_queue_event(pu_create_event_set(), AQ_ToSF);


    lib_timer_clock_t files_resend_clock = {0};
    lib_timer_init(&files_resend_clock, DEFAULT_TO_FOR_FILES_RESEND);

    lib_timer_clock_t dir_clean_clock = {0};   /* timer for md/sd directories cleanup */
    lib_timer_init(&dir_clean_clock, DEFAULT_TO_FOR_DIR_CLEANUP);

    ac_cam_resend_queue_t* resend_queue = ac_cam_create_not_sent();

    size_t len = sizeof(q_msg);

    while(!is_stop) {
        pu_queue_event_t ev;

        switch (ev = pu_wait_for_queues(events, 1)) {
            case AQ_ToSF: {
                while (pu_queue_pop(from_main, q_msg, &len)) {
                    pu_log(LL_INFO, "%s: received from Agent: %s ", PT_THREAD_NAME, q_msg);
                    fld_t* fld = open_flist(q_msg);
                    if(fld) {
                        fd_t fd;
                        while(get_next_f(fld, &fd)) {
                            unsigned long f_id = send_file(fd);
                            if(f_id >= 0) { /* id < 0 - file not found. nothing to do*/
                                if(!f_id) ac_cam_add_not_sent(resend_queue, fd.type, fd.name);
                                send_alert_to_proxy(fd.type, f_id);
                            }
                        }
                        close_flist(fld);
                    }
                    else {
                        pu_log(LL_ERROR, "%s: Can't get files list from %s ", PT_THREAD_NAME, q_msg);
                    }
                    len = sizeof(q_msg);
                 }
            }
                break;
            case AQ_Timeout:
                break;
            case AQ_STOP:
                is_stop = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", PT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait (to server)!", PT_THREAD_NAME, ev);
                break;
        }
/*1. Try to resend not sent files */
        if(lib_timer_alarm(files_resend_clock)) {
            resend_queue = resend(resend_queue);
            lib_timer_init(&files_resend_clock, DEFAULT_TO_FOR_FILES_RESEND);
        }
/*2. MD/SD directories cleanup */
        if(lib_timer_alarm(dir_clean_clock)) {
            ac_delete_old_dirs();
            clean_snap_n_video();
            lib_timer_init(&dir_clean_clock, DEFAULT_TO_FOR_DIR_CLEANUP);
        }

    }
    ac_cam_delete_not_sent(resend_queue);
    return NULL;
}

int at_start_sf() {
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &thread_function, NULL)) return 0;
    return 1;

}
void at_stop_sf() {
    void *ret;

    at_set_stop_sf();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}
void at_set_stop_sf() {
    is_stop = 1;
}



