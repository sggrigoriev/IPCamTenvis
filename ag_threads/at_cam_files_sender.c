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
#include "ag_db_mgr.h"

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
    time_t event_time;
} fld_t;

typedef enum {
    SF_UNDEF= 0, SF_ACT_SEND=1, SF_ACT_SEND_IF_TIME=2, SF_ACT_UPLOAD=3, SF_ACT_UPDATE=4, SF_ACT_SIZE
} sf_action_t;
const char* sf_action_name[SF_ACT_SIZE] = {"???", "SF_ACT_SEND", "SF_ACT_SEND_IF_TIME", "SF_ACT_UPLOAD", "SF_ACT_UPDATE"
};
static sf_action_t string2sfa(const char* str) {
    sf_action_t i;
    if(!str) return SF_UNDEF;
    for(i = SF_UNDEF; i < SF_ACT_SIZE; i++) if(!strcmp(str, sf_action_name[i])) return i;
    return SF_UNDEF;
}
typedef struct {
    sf_action_t action;
    const char* name;
    const char* ext;
    char type;
    size_t size;
    time_t event_time;
/******/
    char upl_url[1024];
    char headers[1024];
    unsigned long fileRef;
    time_t url_creation_time;
    time_t file_creation_time;
} fd_t;
static void print_fd(const fd_t* fd) {
    pu_log(LL_DEBUG, "%s:\naction = %s\nname = %s\next = %s\ntype=%c\nsize = %lu\nevent_time = %lu\nupl_url = %s\nheaders = %s\nfileRef = %lu\nurl_creation_time = %lu\nfile_creation_time = %lu",
            __FUNCTION__, sf_action_name[fd->action], fd->name, fd->ext, fd->type, fd->size, fd->event_time, fd->upl_url, fd->headers, fd->fileRef, fd->url_creation_time, fd->file_creation_time);
}
typedef enum {
    SF_RC_SENT_OK,      /* 0        File sent */
    SF_RC_EARLY,        /* 1    Should wait until sending */
    SF_RC_1_FAIL,       /* 2    Get URL step failed */
    SF_RC_2_FAIL,       /* 3    File upload failed */
    SF_RC_3_FAIL,       /* 4    File attr update failed */
    SF_RC_NOT_FOUND,    /* 5    No such file or directory */
    SF_RC_NO_SPACE,     /* 6    No space to load */
    SF_RC_URL_TOO_OLD,  /* 7    URL was created more than DEFAULT_TO_URL_DEAD ago */
    SF_RC_FREF_TOO_OLD, /* 8    File was erased after DEFAULT_TO_FILE_DEAD time on file server */
    SF_RC_BAD_FILE      /* 9    Currently - zero size. */
} sf_rc_t;
static sf_action_t calc_action(sf_rc_t rc) {
    switch (rc) {
        case SF_RC_SENT_OK:      /* File sent */
        case SF_RC_NOT_FOUND:    /* No such file or directory */
        case SF_RC_NO_SPACE:     /* No space to load */
        case SF_RC_BAD_FILE:     /* 9    Currently - zero size. */
            return SF_UNDEF;
        case SF_RC_EARLY:        /* Should wait until sending */
            return SF_ACT_SEND_IF_TIME;
        case SF_RC_1_FAIL:       /* Get URL step failed */
        case SF_RC_URL_TOO_OLD:  /* URL was created more than DEFAULT_TO_URL_DEAD ago */
        case SF_RC_FREF_TOO_OLD:  /* File was erased after DEFAULT_TO_FILE_DEAD time on file server */
            return SF_ACT_SEND;
        case SF_RC_2_FAIL:       /* File upload failed */
            return SF_ACT_UPLOAD;
        case SF_RC_3_FAIL:       /* File attr update failed */
            return SF_ACT_UPDATE;
        default:
            break;
    }
    return SF_UNDEF;
}
static int type2cloud(char t) {
    switch (t) {
        case 'M':
            return 1;
        case 'P':
            return 2;
        case 'S':
            return 3;
        default:
            pu_log(LL_ERROR, "%s: Unknown media type %c", t);
            break;
    }
    return 0;
}
/******************************************************************/
/*         Send files functions                                   */
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

static void file_delete(const char* name) {
    if(!unlink(name)) {
        pu_log(LL_DEBUG, "%s: File %s deleted", __FUNCTION__, name);
    }
    else {
        pu_log(LL_ERROR, "%s: error deletion %s: %d - %s", __FUNCTION__, name, errno, strerror(errno));
    }
}

static struct curl_slist* make_getSF_header(const fd_t* in_par, struct curl_slist* sl) {
    char buf[512]={0};
    const char* content;
    switch (type2cloud(in_par->type)) {
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
            pu_log(LL_ERROR, "%s: Unknown content type %d", __FUNCTION__, in_par->type);
            return NULL;
            break;
    }
    snprintf(buf, sizeof(buf)-1, "Content-Type: %s%s", content, in_par->ext);
    if(sl = curl_slist_append(sl, buf), !sl) return NULL;

    snprintf(buf, sizeof(buf)-1, "PPCAuthorization: esp token=%s", ag_getProxyAuthToken());
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

static const char* make_getSF_url(char* buf, size_t size, fd_t* in_par) {
    snprintf(buf, size,
             "%s/%s?proxyId=%s&deviceId=%s&ext=%s&expectedSize=%zu&thumbnail=false&rotate=%d&incomplete=false&uploadUrl=true&type=%d",
             ag_getMainURL(),
             DEFAULT_FILES_UPL_PATH,
             ag_getProxyID(),
             ag_getProxyID(),
             in_par->ext,
             in_par->size,
             (type2cloud(in_par->type)==2)?180:0,       /* Turn on 180 if image */
             type2cloud(in_par->type)
    );
    return buf;
}
static const char* make_updSF_url(char* buf, size_t size, fd_t* in_par) {
    snprintf(buf, size,
             "%s/%s/%lu?proxyId=%s&incomplete=false",
             ag_getMainURL(),
             DEFAULT_FILES_UPL_PATH,
             in_par->fileRef,
             ag_getProxyID()
    );
    return buf;
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
/*
 * Return 1 if Ok
 *          0 if error
 *          -1 if no space
 */
static int parse_cloud_answer(const char* ptr, fd_t* in_par) {
    int ret = 0;
    cJSON* obj = cJSON_Parse(ptr);
    if(!obj) {
        pu_log(LL_ERROR, "%s: error %s parsing", __FUNCTION__, ptr);
        return 0;
    }
    cJSON* action = cJSON_GetObjectItem(obj, "filesAction");
    if(action && (action->valueint == 2)) { /* No space to upload */
        pu_log(LL_WARNING, "%s: No space to upload file. File %s will be deleted.", __FUNCTION__, in_par->name);
        ret = -1;
        goto on_error;
    }
    cJSON* url = cJSON_GetObjectItem(obj, "contentUrl");
    if(!url) {
        pu_log(LL_ERROR, "%s: \"contentUrl\" field not found", __FUNCTION__);
        goto on_error;
    }
    pu_log(LL_DEBUG, "%s: content url = %s", __FUNCTION__, url->valuestring);
    strncpy(in_par->upl_url, url->valuestring, sizeof(in_par->upl_url));

    cJSON* hdrs = cJSON_GetObjectItem(obj, "uploadHeaders");
    if(!hdrs) {
        pu_log(LL_ERROR, "%s: \"uploadHeaders\" field not found", __FUNCTION__);
        goto on_error;
    }
    char *hd = cJSON_PrintUnformatted(hdrs);
    pu_log(LL_DEBUG, "%s: upload headers = %s", __FUNCTION__, hd);
    strncpy(in_par->headers, hd, sizeof(in_par->headers));
    free(hd);

    cJSON* file_id = cJSON_GetObjectItem(obj, "fileRef");
    if(!file_id) {
        pu_log(LL_ERROR, "%s: \"fileRef\" field not foind", __FUNCTION__);
        goto on_error;
    }
    in_par->fileRef = (unsigned long)(file_id->valueint);
    pu_log(LL_DEBUG, "%s: fileId = %lu", __FUNCTION__, in_par->fileRef);
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
/*
 * Return 1 if OK, 0 if error, -1 if no space
 */
static int getSF_URL(fd_t* in_par) {
    int ret = 0;
    char url_cmd[1024]={0};
    struct curl_slist *hs = NULL;
    char* ptr = NULL;


    make_getSF_url(url_cmd, sizeof(url_cmd), in_par);
    pu_log(LL_DEBUG, "%s: URL for request= %s", __FUNCTION__, url_cmd);

    if(hs = make_getSF_header(in_par, hs), !hs) goto on_exit;
    if(ptr = post_n_reply(hs, url_cmd), !ptr) goto on_exit;

    ret = parse_cloud_answer(ptr, in_par);
    if(ret == 0) {
        pu_log(LL_ERROR, "%s: Error parsing answer from cloud %s", __FUNCTION__, ptr);
        goto on_exit;
    }
    else if(ret == -1) goto on_exit;

    ret = 1;
on_exit:
    if(ptr)free(ptr);
    if(hs) curl_slist_free_all(hs);
    if(ret!=1) {
        in_par->upl_url[0] = '\0';
        in_par->headers[0] = '\0';
        in_par->fileRef = 0;
    }
    return ret;
}
/*
 * Return 1 if OK, 0 if error, 2 if file not found
 */
static int sendFile(fd_t* in_par) {
    int ret = 0;

    struct curl_slist *hs=NULL;

    char err_b[CURL_ERROR_SIZE]= {0};
    CURL *curl;
    CURLcode res;

    char* ptr = NULL;
    size_t sz=0;
    FILE* fp = NULL;
    FILE* fd = NULL;

    if(curl = init(), !curl) return 0;

    if(hs = make_getSF_header(in_par, hs), !hs) goto on_exit;
    if(hs = make_SF_header(in_par->headers, hs), !hs) goto on_exit; /* Add file server headers to the common one */

    if(fd = fopen(in_par->name, "rb"), !fd) { /* open file to upload */
        pu_log(LL_ERROR, "%s: Open %s file error %d-%s", __FUNCTION__, in_par->name, errno, strerror(errno));
        if(errno == ENOENT) ret = 2;   /* No such file. It was sent already */
        goto on_exit;
    }

    if(fp = open_memstream(&ptr, &sz), !fp) {
        pu_log(LL_ERROR, "%s: Error open memstream: %d - %s", __FUNCTION__, errno, strerror(errno));
        goto on_exit;
    }
    if(res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_b), res != CURLE_OK) goto on_exit;
    if(res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs), res != CURLE_OK) goto on_exit;
    if(res = curl_easy_setopt(curl, CURLOPT_URL, in_par->upl_url), res != CURLE_OK) goto on_exit;
    if(res = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L), res != CURLE_OK) goto on_exit;

    if(res = curl_easy_setopt(curl, CURLOPT_READDATA, fd), res != CURLE_OK) goto on_exit;
    if(res = curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)in_par->size), res != CURLE_OK) goto on_exit;
    if(res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL), res != CURLE_OK) goto on_exit;
    if(res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp), res != CURLE_OK) goto on_exit;

    if(res = curl_easy_perform(curl), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Curl error %s", __FUNCTION__, curl_easy_strerror(res));
        goto on_exit;
    }

    fflush(fp);
    if(!ptr) {
        pu_log(LL_ERROR, "%s: Cloud returns empty answer on %s request", __FUNCTION__, in_par->upl_url);
        goto on_exit;
    }
    pu_log(LL_DEBUG, "%s: Answer = %s", __FUNCTION__, ptr);

    ret = 1;
on_exit:
    if(fd) fclose(fd);
    if(fp) fclose(fp);
    if(ptr) free(ptr);
    curl_easy_cleanup(curl);
    if(hs) curl_slist_free_all(hs);
    return ret;
}
static int sendSF_update(fd_t* in_par) {
    int ret = 0;
    struct curl_slist *hs = NULL;
    char* ptr = NULL;
    char url_cmd[1024]={0};

    make_updSF_url(url_cmd, sizeof(url_cmd), in_par);
    pu_log(LL_DEBUG, "%s: URL = %s", __FUNCTION__, url_cmd);

    if(hs = make_SFupdate_header(hs, ag_getProxyAuthToken()), !hs) goto on_exit;

    if(ptr = put_n_reply(hs, url_cmd), !ptr) goto on_exit;
    if(!is_cloud_answerOK(ptr)) goto on_exit;

    ret = 1;
on_exit:
    if(ptr) free(ptr);
    if(hs) curl_slist_free_all(hs);
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
/*
 * There are two variants of JSON: message sent from Agent
 * {"name": "sendFiles", "type": <fileTypeString", "filesList": ["<filename>", ..., "<filename>"]}
 *                      or
 * {"name": "sendFiles", "type": <fileTypeString", "timestamp": <end_date>, "filesList": ["<filename>", ..., "<filename>"]}
 * Mesage sent from resend queue
 * [e1,..., eN]
 * ei is
 * {"action":<string>, "file_name":<string>, "file_type":<string>, "file_time":<number>,
 * "url":<string>, "headers":<string>, "url_time":<number>,"fileRef":<number>, "fileRef_time":<number>}
 */
static fld_t* open_flist(const char* msg) {
    fld_t* ret = NULL;
    time_t timestamp = 0;
    char file_type = '\0';
    cJSON* files_array=NULL;

    cJSON* obj = cJSON_Parse(msg);
    if(!obj) return NULL;
    if(obj->type == cJSON_Object) { /* Message from Agent */
        cJSON* type = cJSON_GetObjectItem(obj, "type");
        if(!type || (type->type != cJSON_String)) goto err;
        files_array = cJSON_GetObjectItem(obj, "filesList");
        if(!files_array || (files_array->type != cJSON_Array)) goto err;
        cJSON* ts = cJSON_GetObjectItem(obj, "timestamp");
        timestamp = (ts)?ts->valueint:0;
        file_type = type->valuestring[0];
    }
    else if(obj->type == cJSON_Array) { /* Message from resend queue */
        files_array = obj;
    }
    else {
        goto err;
    }

    ret = calloc(sizeof(fld_t), 1);
    if(!ret) {
        pu_log(LL_ERROR, "%s: not enough memory", __FUNCTION__);
        goto err;
    }
    ret->root = obj;
    ret->arr = files_array;
    ret->idx = 0;
    ret->total = cJSON_GetArraySize(files_array);
    ret->ft = file_type;
    ret->event_time = timestamp;
    return ret;
err:
    cJSON_Delete(obj);
    return ret;
}
/*
 * Array element could contain or just <string> as file name
 * {"action":<string>, "file_name":<string>, "file_type":<string>, "file_time":<number>,
 * "url":<string>, "headers":<string>, "url_time":<number>,"fileRef":<number>, "fileRef_time":<number>}
 */
static int get_next_f(fld_t* fld, fd_t* fd) {
    if(fld->idx < fld->total) {
        cJSON* item = cJSON_GetArrayItem(fld->arr, fld->idx);

        if(item->type == cJSON_String) {                            /* Got simple case */
            fd->action = (fld->event_time)?SF_ACT_SEND_IF_TIME:SF_ACT_SEND;
            fd->name = cJSON_GetArrayItem(fld->arr, fld->idx)->valuestring;
            fd->type = fld->ft;
            fd->upl_url[0] = '\0';
            fd->headers[0] = '\0';
            fd->fileRef = 0;
            fd->event_time = fld->event_time;
            fd->file_creation_time = 0;
            fd->url_creation_time = 0;
        }
        else {                                                      /* Resend part */
            fd->action = string2sfa(cJSON_GetObjectItem(item, "action")->valuestring);
            fd->name = cJSON_GetObjectItem(item, "file_name")->valuestring;
            fd->type = cJSON_GetObjectItem(item, "file_type")->valuestring[0];
            fd->event_time = cJSON_GetObjectItem(item, "file_time")->valueint;
            snprintf(fd->upl_url, sizeof(fd->upl_url), "%s",cJSON_GetObjectItem(item, "url")->valuestring);
            char* hdr = (cJSON_GetObjectItem(item, "headers"))?cJSON_PrintUnformatted(cJSON_GetObjectItem(item, "headers")):NULL;
            if(hdr) {
                snprintf(fd->headers, sizeof(fd->headers), "%s", hdr);
                free(hdr);
            }
            else {
                fd->headers[0] = '\0';
            }
            fd->url_creation_time = cJSON_GetObjectItem(item, "url_time")->valueint;
            fd->fileRef = (unsigned long)(cJSON_GetObjectItem(item, "fileRef")->valueint);
            fd->file_creation_time = cJSON_GetObjectItem(item, "fileRef_time")->valueint;
        }
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
 * Return sf_src_t
 */
static const sf_rc_t send_file(fd_t* fd) {
    sf_rc_t ret = SF_RC_SENT_OK;
    int rc;
    print_fd(fd);
    if(!fd->size) {
        pu_log(LL_ERROR, "%s: File %s got zero size! Will be deleted", __FUNCTION__, fd->name);
        return SF_RC_BAD_FILE;
    }
    switch (fd->action) {
        case SF_ACT_SEND_IF_TIME:
            if ((time(NULL) - fd->event_time) < ag_db_get_int_property(AG_DB_STATE_MD_COUNTDOWN)) {
                pu_log(LL_WARNING, "%s: Too early to send %s. Queued.", __FUNCTION__, fd->name);
                ret = SF_RC_EARLY;
                break;
            }
        case SF_ACT_SEND:
            rc = getSF_URL(fd);
            if (rc == 0) {
                ret = SF_RC_1_FAIL;
                break;
            } else if (rc == -1) {
                ret = SF_RC_NO_SPACE;
                break;
            }

            fd->url_creation_time = time(NULL);
            pu_log(LL_DEBUG, "%s: URL = %s, UPL_HDRS = %s", __FUNCTION__, fd->upl_url, fd->headers);
        case SF_ACT_UPLOAD:
            if ((time(NULL) - fd->url_creation_time) > DEFAULT_TO_URL_DEAD) {
                pu_log(LL_WARNING, "%s: Too old URL. Queued.", __FUNCTION__);
                ret = SF_RC_URL_TOO_OLD;
                break;
            }
            rc = sendFile(fd);
            if (rc == 0) {
                pu_log(LL_ERROR, "%s: error upload file %s", __FUNCTION__, fd->name);
                ret = SF_RC_2_FAIL;
                break;
            } else if (rc == 2) {
                pu_log(LL_WARNING, "%s: No file %s", __FUNCTION__, fd->name);
                ret = SF_RC_NOT_FOUND;
                break;
            }
            fd->file_creation_time = time(NULL);
            pu_log(LL_DEBUG, "%s: file %s sent OK", __FUNCTION__, fd->name);
        case SF_ACT_UPDATE:
            if ((time(NULL) - fd->file_creation_time) > DEFAULT_TO_FILE_DEAD) {
                pu_log(LL_WARNING, "%s: fileRef %d too old. Queued", __FUNCTION__, fd->fileRef);
                ret = SF_RC_FREF_TOO_OLD;
                break;
            }
            if (!sendSF_update(fd)) {
                pu_log(LL_ERROR, "%s: error sending completion update for file %s", __FUNCTION__, fd->name);
                ret = SF_RC_3_FAIL;
                break;
            }
            break;
        default:
            pu_log(LL_ERROR, "%s: Internal error. Unknown action type = %d.", __FUNCTION__, fd->action);
            break;
    }
    return ret;
}
/*
 * return queue with all not sent files
 * In ultimate case all this crap will be deleted after DEFAULT_TO_FOR_DIR_CLEANUP timeout
 * array contains object:
 * {"action":<string>, "file_name":<string>, "file_type":<string>, "file_time":<number>,
 * "url":<string>, "headers":<string>, "url_time":<number>,"fileRef":<number>, "fileRef_time":<number>}
 */
static cJSON* ac_cam_create_not_sent() {
    cJSON* ret = cJSON_CreateArray();
    if(!ret) {
        pu_log(LL_ERROR, "%s: Not enough memory", __FUNCTION__);
    }
    return ret;
}
static int ac_cam_add_not_sent(cJSON* q, fd_t* fd, sf_rc_t rc) {
    pu_log(LL_DEBUG, "%s: Going to add file %s type %c reason %d", __FUNCTION__, fd->name, fd->type, rc);
    if(!q) {
        pu_log(LL_ERROR, "%s: queue or name is NULL. No candy - no Masha!", __FUNCTION__);
        return 0;
    }
    cJSON* item = cJSON_CreateObject();
    cJSON_AddItemToObject(item, "action", cJSON_CreateString(sf_action_name[calc_action(rc)]));
    cJSON_AddItemToObject(item, "file_name", cJSON_CreateString(fd->name));
    char buf[2]={0}; buf[0] = fd->type;
    cJSON_AddItemToObject(item, "file_type", cJSON_CreateString(buf));
    cJSON_AddItemToObject(item, "file_time", cJSON_CreateNumber(fd->event_time));
    cJSON_AddItemToObject(item, "url", cJSON_CreateString(fd->upl_url));
    if(fd->headers[0] != '\0') {
        cJSON_AddItemToObject(item, "headers", cJSON_Parse(fd->headers));
    }
    cJSON_AddItemToObject(item, "url_time", cJSON_CreateNumber(fd->url_creation_time));
    cJSON_AddItemToObject(item, "fileRef", cJSON_CreateNumber(fd->fileRef));
    cJSON_AddItemToObject(item, "fileRef_time", cJSON_CreateNumber(fd->file_creation_time));

    cJSON_AddItemToArray(q, item);
    char* txt = cJSON_PrintUnformatted(q);
    pu_log(LL_DEBUG, "%s: resend_queue = %s", __FUNCTION__, txt);
    free(txt);
    return 1;
}
static void ac_cam_delete_not_sent(cJSON* q) {
    if(q) cJSON_Delete(q);
}

static cJSON* resend(cJSON* q) {
    if(!q) {
        pu_log(LL_ERROR, "%s: Internal error: Resend Queue is NULL!", __FUNCTION__);
        return q;
    }
    if(!cJSON_GetArraySize(q)) return q;

    char* txt = cJSON_PrintUnformatted(q);
    if(txt) {
        pu_queue_push(from_main, txt, strlen(txt) + 1);
        pu_log(LL_DEBUG, "%s: SF resends %s", __FUNCTION__, txt);
        free(txt);
    }
    cJSON_Delete(q);
    return ac_cam_create_not_sent();
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

    cJSON* resend_queue = ac_cam_create_not_sent();

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
                        sf_rc_t rc;
                        while(get_next_f(fld, &fd)) {
                            switch (rc=send_file(&fd)) {
                                case SF_RC_SENT_OK:
                                    send_alert_to_proxy(fd.type, fd.fileRef);
                                case SF_RC_NO_SPACE:
                                case SF_RC_BAD_FILE:
                                    file_delete(fd.name);
                                    break;
                                case SF_RC_EARLY:
                                case SF_RC_1_FAIL:
                                case SF_RC_2_FAIL:
                                case SF_RC_3_FAIL:
                                case SF_RC_URL_TOO_OLD:
                                case SF_RC_FREF_TOO_OLD:
                                    ac_cam_add_not_sent(resend_queue, &fd, rc);
                                    break;
                                case SF_RC_NOT_FOUND:
                                    /* Nothing to do */
                                default:
                                    break;
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



