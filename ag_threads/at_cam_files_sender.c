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
#include <sys/types.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <au_string/au_string.h>

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

#define ATI_ACTION      "action"
#define ATI_PATH        "path"
#define ATI_FILE_NAME   "file_name"
#define ATI_FILE_EXT    "file_ext"
#define ATI_FILE_TYPE   "file_type"
#define ATI_FILE_SIZE   "file_size"
#define ATI_FILE_TIME   "file_time"

#define ATI_URL       "url"
#define ATI_HEADERS   "headers"
#define ATI_URL_TIME  "url_time"
#define ATI_FILE_REF  "file_ref"
#define ATI_FILE_REF_TIME "file_ref_time"

static volatile int is_stop = 0;
static pthread_t id;
static pthread_attr_t attr;

static pu_queue_t* from_main;
static pu_queue_t* to_proxy;
static pu_queue_msg_t q_msg[LIB_HTTP_MAX_MSG_SIZE];    /* Buffer for messages received */

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
    char path[PATH_MAX];    /* "" if undefiled */
    char name[256];
    char ext[10];
    char type[3];
    size_t size;                /* 0 if undef */
    time_t event_time;
/******/
    char upl_url[1024];
    char headers[1024];
    unsigned long fileRef;
    time_t url_creation_time;
    time_t file_creation_time;
} fd_t;
static void print_fd(const fd_t* fd) {
    pu_log(LL_DEBUG, "%s:\naction = %s\npath = %s\nname = %s\next = %s\ntype=%s\nsize = %lu\nevent_time = %lu\nupl_url = %s\nheaders = %s\nfileRef = %lu\nurl_creation_time = %lu\nfile_creation_time = %lu",
                       __FUNCTION__, sf_action_name[fd->action], fd->path, fd->name, fd->ext, fd->type, fd->size, fd->event_time, fd->upl_url, fd->headers, fd->fileRef, fd->url_creation_time, fd->file_creation_time);
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
        case '\0':
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

static struct curl_slist* make_getSF_header(const fd_t* in_par, struct curl_slist* sl) {
    char buf[512]={0};
    const char* content;
    switch (type2cloud(in_par->type[0])) {
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
             (type2cloud(in_par->type[0])==2)?180:0,       /* Turn on 180 if image */
             type2cloud(in_par->type[0])
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

    if(res = curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 0L), res != CURLE_OK) goto on_exit;
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
            if (time(NULL) < fd->event_time) {
                pu_log(LL_WARNING, "%s: Too early to send %s. Queued.", __FUNCTION__, fd->name);
                ret = SF_RC_EARLY;
                break;
            }
        case SF_ACT_SEND:
            rc = getSF_URL(fd);
            if (rc == 0) {
                ret = SF_RC_1_FAIL;
                fd->event_time = time(NULL)+DEFAULT_MP4_PROCESSING;
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
                fd->event_time = time(NULL)+DEFAULT_MP4_PROCESSING;
                break;
            }
            rc = sendFile(fd);
            if (rc == 0) {
                pu_log(LL_ERROR, "%s: error upload file %s", __FUNCTION__, fd->name);
                ret = SF_RC_2_FAIL;
                fd->event_time = time(NULL)+DEFAULT_MP4_PROCESSING;
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
                fd->event_time = time(NULL)+DEFAULT_MP4_PROCESSING;
                break;
            }
            if (!sendSF_update(fd)) {
                pu_log(LL_ERROR, "%s: error sending completion update for file %s", __FUNCTION__, fd->name);
                ret = SF_RC_3_FAIL;
                fd->event_time = time(NULL)+DEFAULT_MP4_PROCESSING;
                break;
            }
            break;
        default:
            pu_log(LL_ERROR, "%s: Internal error. Unknown action type = %d.", __FUNCTION__, fd->action);
            break;
    }
    return ret;
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
    const char *msg = ao_cmd_cloud_msg(ag_getProxyID(), ao_cmd_cloud_alerts(ag_getProxyID(), a_num, ev, f_num), NULL, NULL, buf, sizeof(buf));
    if(!msg) {
        pu_log(LL_ERROR, "%s: message to cloud exceeds max size %d. Ignored", __FUNCTION__, LIB_HTTP_MAX_MSG_SIZE);
        return;
    }
    pu_queue_push(to_proxy, msg, strlen(msg)+1);
}
/**********************************************************************************/
static int ymd_tm2int(struct tm* date) {
/*     int dir_time =  */
    return date->tm_year*10000 + date->tm_mon*100 + date->tm_mday;
}
static int ymd2int(int y, int m, int d) {
    return (y-1900)*10000 + (m-1)*100 + d;
}
static int hms_time_t2int(time_t t) {
    struct tm d;
    gmtime_r(&t, &d);
    return d.tm_sec + d.tm_min*100 + d.tm_hour*10000;
}
static int hms2int(int h, int m, int s) {
    return s + m*100 + h*10000;
}
/*
 * Returns name YYYY-MM-DD made from today's date
 */
static const char* make_today_dir_name(char* buf, size_t size) {
    time_t now = time(NULL);
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    snprintf(buf, size, "%04d-%02d-%02d", tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday);
    return buf;
}
/*
 * Return the deepest direcory from path.
 * NB! path does not include file
 */
static const char* get_last_dir(const char* path) {
    size_t n = strlen(path);
    while(n && path[n] != '/') n--;
    return (path[n] == '/')?path+n+1:path+n;
}
/*
 * concat path & name
 * NB! returned memory should be freed afetr use!
 */
static char* get_full_name(const char* path, const char* name) {
    char buf[PATH_MAX]={0};
    snprintf(buf, sizeof(buf)-1, "%s/%s", path, name);

    char* ret = strdup(buf);
    if(!ret) {
        pu_log(LL_ERROR, "%s: Not enugh memory", __FUNCTION__);
    }
    return ret;
}
/*
 * rmdir <name> -r
 * https://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c
 * return 0 if Ok
 */
static int remove_dir(const char* path_n_name) {

    pu_log(LL_DEBUG, "%s: Remove %s", __FUNCTION__, path_n_name);

    DIR *d = opendir(path_n_name);
    if(!d) return 0;

    struct dirent *p;
    while (p=readdir(d), p != NULL) {
        char* full_name = NULL;

        if(!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue; /* Skip the names "." and ".." as we don't want to recurse on them. */

        if(full_name = get_full_name(path_n_name, p->d_name), !full_name) goto on_error;

        struct stat statbuf;
        if(lstat(full_name, &statbuf)) {
            pu_log(LL_ERROR, "%s: stat %s error %d - %s", __FUNCTION__, full_name, errno, strerror(errno));
            goto on_error;
        }

        if(S_ISDIR(statbuf.st_mode)) {    /* Directory again */
            if(!remove_dir(full_name)) goto on_error;
        }
        else if(unlink(full_name)) {
            pu_log(LL_ERROR, "%s: unlink %s error %d - %s", __FUNCTION__, full_name, errno, strerror(errno));
            goto on_error;
        }
        on_error:
        if(full_name) free(full_name);
    }
    closedir(d);

    return rmdir(path_n_name);
}
/*
 * Converts structure to JSON
 * item:
* {"action":<string>, "path":<string>, "file_name":<string>, "file_ext":<string>, "file_type":<string>,"file_size":<number>, "file_time":<number>,
* "url":<string>, "headers":<string>, "url_time":<number>,"file_ref":<number>, "file_ref_time":<number>}
 * NB! if fd fileld is undefined -> no fileld in item as well!
 */
static int fd2json(const fd_t* task, cJSON** item) {
    int ret = 0;

    cJSON* i = cJSON_CreateObject();

    if(!i) goto on_exit;
/* action */
    cJSON_AddItemToObject(i, ATI_ACTION, cJSON_CreateString(sf_action_name[task->action]));
/* path */
    cJSON_AddItemToObject(i, ATI_PATH, cJSON_CreateString(task->path));
/* file_name */
    cJSON_AddItemToObject(i, ATI_FILE_NAME, cJSON_CreateString(task->name));
/* file_ext */
    cJSON_AddItemToObject(i, ATI_FILE_EXT, cJSON_CreateString(task->ext));
/* file_type */
    cJSON_AddItemToObject(i, ATI_FILE_TYPE, cJSON_CreateString(task->type));
/* file_size */
    cJSON_AddItemToObject(i, ATI_FILE_SIZE, cJSON_CreateNumber(task->size));
/* file_time */
    if(!task->event_time)
        cJSON_AddItemToObject(i, ATI_FILE_TIME, cJSON_CreateNumber(UINT_MAX));
    else
        cJSON_AddItemToObject(i, ATI_FILE_TIME, cJSON_CreateNumber(task->event_time));
/* url */
    if(strlen(task->upl_url)) cJSON_AddItemToObject(i, ATI_URL, cJSON_CreateString(task->upl_url));
/* headers */
    if(strlen(task->headers)) cJSON_AddItemToObject(i, ATI_HEADERS, cJSON_CreateString(task->headers));
/* url_time */
    if(task->url_creation_time) cJSON_AddItemToObject(i, ATI_URL_TIME, cJSON_CreateNumber(task->url_creation_time));
/* file_ref */
    if(task->fileRef) cJSON_AddItemToObject(i, ATI_FILE_REF, cJSON_CreateNumber(task->fileRef));
/* file_ref_time */
    if(task->file_creation_time) cJSON_AddItemToObject(i, ATI_FILE_REF_TIME, cJSON_CreateNumber(task->file_creation_time));

    ret = 1;
    on_exit:
    if (!ret) {
        if(i) cJSON_Delete(i);
        i = NULL;
    }
    *item = i;
    return ret;
}
/*
 * Converts JSON to structure
 * Return 0 if error, 1 if OK
* {"action":<string>, "path":<string>, "file_name":<string>, "file_ext":<string>, "file_type":<string>,"file_size":<number>, "file_time":<number>,
* "url":<string>, "headers":<string>, "url_time":<number>,"file_ref":<number>, "file_ref_time":<number>}
 */
static int json2fd(cJSON* item, fd_t* task) {
    cJSON* i;
/* action */
    if(i=cJSON_GetObjectItem(item, ATI_ACTION), !i) {
        pu_log(LL_ERROR, "%s: Item %s not found. task ignored", __FUNCTION__, ATI_ACTION);
        return 0;
    }
    task->action = string2sfa(i->valuestring);
/* path */
    if(i=cJSON_GetObjectItem(item, ATI_PATH), !i) {
        pu_log(LL_ERROR, "%s: Item %s not found. task ignored", __FUNCTION__, ATI_PATH);
        return 0;
    }
    strncpy(task->path, i->valuestring, sizeof(task->path));
/* file_name */
    if(i=cJSON_GetObjectItem(item, ATI_FILE_NAME), !i) {
        pu_log(LL_ERROR, "%s: Item %s not found. task ignored", __FUNCTION__, ATI_FILE_NAME);
        return 0;
    }
    strncpy(task->name, i->valuestring, sizeof(task->name));
/* file_ext */
    if(i=cJSON_GetObjectItem(item, ATI_FILE_EXT), !i) {
        pu_log(LL_ERROR, "%s: Item %s not found. task ignored", __FUNCTION__, ATI_FILE_EXT);
        return 0;
    }
    strncpy(task->ext, i->valuestring, sizeof(task->ext));
/* file_type */
    if(i=cJSON_GetObjectItem(item, ATI_FILE_TYPE), !i) {
        pu_log(LL_ERROR, "%s: Item %s not found. task ignored", __FUNCTION__, ATI_FILE_TYPE);
        return 0;
    }
    strncpy(task->type, i->valuestring, sizeof(task->type));
/* file_size */
    if(i=cJSON_GetObjectItem(item, ATI_FILE_SIZE), !i) {
        pu_log(LL_ERROR, "%s: Item %s not found. task ignored", __FUNCTION__, ATI_FILE_SIZE);
        return 0;
    }
    task->size = (size_t)i->valueint;
/* file_time */
    if(i=cJSON_GetObjectItem(item, ATI_FILE_TIME), i) {
        pu_log(LL_ERROR, "%s: Item %s not found. task ignored", __FUNCTION__, ATI_FILE_TIME);
        return 0;
    }
    if((unsigned int)i->valueint == UINT_MAX)
        task->event_time = 0;
    else
        task->event_time = i->valueint;
/* url */
    if(i=cJSON_GetObjectItem(item, ATI_URL), i)
        strncpy(task->upl_url, i->valuestring, sizeof(task->upl_url));
    else
        task->upl_url[0] = '\0';
/* headers */
    if(i=cJSON_GetObjectItem(item, ATI_HEADERS), i)
        strncpy(task->headers, i->valuestring, sizeof(task->headers));
    else
        task->headers[0] = '\0';
/* url_time */
    if(i=cJSON_GetObjectItem(item, ATI_URL_TIME), i)
        task->url_creation_time = i->valueint;
    else
        task->url_creation_time = 0;
/* file_ref */
    if(i=cJSON_GetObjectItem(item, ATI_FILE_REF), i)
        task->fileRef = (unsigned long)i->valueint;
    else
        task->fileRef = 0;
/* file_ref_time */
    if(i=cJSON_GetObjectItem(item, ATI_FILE_REF_TIME), i)
        task->file_creation_time = i->valueint;
    else
        task->file_creation_time = 0;
    print_fd(task);
    return 1;
}
static const char* g_prefix;
static const char* g_postfix;
static const char* g_ext;
static time_t g_start;
static time_t g_stop;
/*
 *
 * <pref>HHMMSS[<postf>].<ext>
 * <pref> - 2 sym
 * hhmmss - 6 dig
 * postf - 1 sym or 0
 * ext - up to el-1
 */
static int parse_fname(const char* name, char* pref, size_t pl, char* postf, size_t pol, char* ext, size_t el, int* h, int* m, int* s) {
    if(!au_getNsyms(&name, pref, pl)) return 0;    /* prefix */

    if(!au_getNdigs(&name, h, 2)) return 0;     /* hours */
    if((*h < 0)||(*h > 23)) return 0;

    if(!au_getNdigs(&name, m, 2)) return 0;     /* minutes */
    if((*m < 0)||(*m > 59)) return 0;

    if(!au_getNdigs(&name, s, 2)) return 0;     /* seconds */
    if((*s < 0)||(*s > 59)) return 0;

    if(!au_getUntil(&name, postf, pol, '.')) return 0;  /* postfix is too big */

    if(*name++ != '.') return 0;

    if(!au_getUntil(&name, ext, el, '\0')) return 0;
    return 1;
}
/*
 * YYYY-MM-DD
 * Return 1 if name got this structure
 */
static int parse_dname(const char* name, int* y, int* m, int* d) {

    if(!au_getNdigs(&name, y, 4)) return 0;

    if(*name++ != '-') return 0;

    if(!au_getNdigs(&name, m, 2)) return 0;
    if((*m < 1) || (*m > 12)) return 0;

    if(*name++ != '-') return 0;

    if(!au_getNdigs(&name, d, 2)) return 0;
    if((*d < 1) || (*d > 31)) return 0;

    return 1;
}
/*
 * YYYY-MM-DD
 */
int is_dname(const char* name) {
    int y,m,d;
    return parse_dname(name, &y, &m, &d);
}
/*
 * Manages by globally set prefix, postfix & extention.u
 * if NULL - could be any.
 * If len == 0 - should not be presented
 * if len != 0 - should be the same
 * NB-1! Limitaion for prefix: it can't be any! Just nothing or smth!
 * NB-2! Add global's size control! They should not be bigger than local arrays!
 * NB-3! Uses only for old direcories scan! today is out of scope!
 */
static int files_filter(const struct dirent * dn) {
    if(dn->d_type != DT_REG) return 0;

    char prefix[3]={0};
    char postfix[2]={0};
    char ext[10]={0};
    int h, m, s;
/* Size control */
    size_t pref_len = (g_prefix)?strlen(g_prefix)+1:sizeof(prefix);
    size_t post_len = (g_postfix)?strlen(g_postfix)+1:sizeof(postfix);
    size_t ext_len = (g_ext)?strlen(g_ext)+1:sizeof(ext);

    if(!parse_fname(dn->d_name, prefix, pref_len, postfix, post_len, ext, ext_len, &h, &m, &s)) return 0;
    if(pref_len && g_prefix && (strcmp(prefix, g_prefix)!=0)) return 0;
    if(post_len && g_postfix && (strcmp(postfix, g_postfix)!=0)) return 0;
    if(ext_len && g_ext && (strcmp(ext, g_ext)!=0)) return 0;
    if(g_start && (hms_time_t2int(g_start) > hms2int(h,m,s))) return 0;
    if(g_stop && (hms_time_t2int(g_stop) < hms2int(h,m,s))) return 0;
    return 1;
}
int dirs_filter(const struct dirent * dn) {
    if(dn->d_type != DT_DIR) return 0;
    if(!strcmp(dn->d_name, DEFAULT_SNAP_DIR)) return 1;
    char buf[20] = {0};
    make_today_dir_name(buf, sizeof(buf));
    if(!strcmp(dn->d_name, buf)) return 0;      /* Today is out of scope */
    return is_dname(dn->d_name);
}
/*
 * return
 */
static time_t get_item_time(cJSON* item) {
    cJSON* event_time = cJSON_GetObjectItem(item, ATI_FILE_TIME);
    if(!event_time) {
        pu_log(LL_ERROR, "Task time is not set!");
        return 0;
    }
    return event_time->valueint;
}
/*
 * Add item to the queue:
 * if time -> send after tasks with early time
 * if !time -> send first
 */
static cJSON* insert_task(cJSON* item, cJSON* queue) {
    time_t item_time = get_item_time(item);

/******/
    cJSON *c=queue->child;
    if(!c) { /* Insert first: empty array */
        item->prev = NULL;
        item->next = NULL;
        queue->child = item;
    }
    else {
        while(c && (get_item_time(c)>=item_time)) {
            c = c->next;
        }
        if(!c)
            cJSON_AddItemToArray(queue, item);  /* Append queue */
        else {  /* Insert after */
            item->prev = c;
            item->next = c->next;
            c->next = item;
            if(item->next) item->next->prev = item;
        }
    }

/******/
    return queue;
}
/*
 * Add tasks to the queue and return it. Files come from yong to old
 * path - fill path to directory with files to be sent
 * prefix - NULL - any 2 symbols prefix, "" - no prefix, ".." - file should have this prefix (2 chars exactly!)
 * postfix - NULL - any, "" - no postfix, "..." - file should have this postfix (1 char exactly)
 * ext - NULL - any, "..." - file should have this extention
 * !start_date - take all
 * start_date && end_date - take from interval
 */
static cJSON* files(const char* path, const char* prefix, const char* postfix, const char* ext, time_t start_date, time_t end_date, cJSON* queue) {
    fd_t task = {0};
    struct dirent** list = NULL;

    g_prefix = prefix;
    g_postfix = postfix;
    g_ext = ext;
    g_start = start_date;
    g_stop = end_date;

    int rc = scandir(path, &list, files_filter, alphasort);

    if(rc < 0) {
        pu_log(LL_ERROR, "%s: error scandir calling: %d - %s", __FUNCTION__, errno, strerror(errno));
        return queue;
    }

    while(rc--) {
        pu_log(LL_DEBUG, "%s: File found: %s", __FUNCTION__, list[rc]->d_name);
/* Place to fill out the task */
        task.action = (end_date)?SF_ACT_SEND_IF_TIME:SF_ACT_SEND;
        strncpy(task.path, path, sizeof(task.path));
        strncpy(task.name, list[rc]->d_name, sizeof(task.name));
        char pref[3];
        int y,m,d;
        if(!parse_fname(task.name, pref, 3, task.type, sizeof(task.type), task.ext, sizeof(task.ext), &y, &m, &d)) {
            pu_log(LL_ERROR, "%s: error parsing file %s/%s", __FUNCTION__, path, task.name);
            free(list[rc]);
            continue;
        }
        task.event_time = (end_date)?end_date+DEFAULT_MP4_PROCESSING:0;
        struct stat st;
        char buf[PATH_MAX]={0};
        snprintf(buf, sizeof(buf), "%s/%s", path, task.name);
        if(stat(buf, &st)==-1) {
            pu_log(LL_ERROR, "%s: error calling stat %d - %s. File %s ignored", __FUNCTION__, errno, strerror(errno), buf);
            free(list[rc]);
            continue;
        }
        task.size = (size_t)st.st_size;
        cJSON* new_item = NULL;
        fd2json(&task, &new_item);
        if(!new_item) {
            pu_log(LL_ERROR, "%s: file %s can't be added to tasks queue. Ignored", __FUNCTION__, buf);
            free(list[rc]);
            continue;
        }
        if(!queue) {
            queue = cJSON_CreateArray();
        }
        queue = insert_task(new_item, queue);


        free(list[rc]);
     }
    free(list);

    return queue;
}
/*
 * if dir YYYY-MM_DD is not today and has no files to send -> dalete it
 * Return 0 if not empty
 * Return 1 if delete (or at least made attempt to)
 * Called after send_files()
 */
int empty_dir_delete(const char* path) {
    int ret = 0;
    int y, m, d;

    const char* last_dir = get_last_dir(path);
    if(!parse_dname(last_dir, &y, &m, &d)) return 0;

    struct tm now;
    time_t n_ts = time(NULL);
    gmtime_r(&n_ts, &now);

    int dir_time = ymd2int(y, m, d);
    int today = ymd_tm2int(&now);
    if (dir_time < today) {
        cJSON *q = files(path, NULL, NULL, NULL, 0, 0, NULL);
        if (!q || !cJSON_GetArraySize(q)) {
            remove_dir(path);
            ret = 1;
        }
        if(q) cJSON_Delete(q);
    }
    return ret;
}
/*
 * Delete dirs which are older than DEFAULT_TO_DIR_LIFE and return NULL
 * Return queue with files if the dir is not too old but older than today and got files to be sent
 * Return NULL if dir is not too old but empty
 * Called from old_all
 */
static cJSON* old_dirs_delete(const char* path, cJSON* queue) {
    int y, m, d;

    const char* last_dir = get_last_dir(path);
    if(!parse_dname(last_dir, &y, &m, &d)) return 0;

    struct tm now;
    time_t n_ts = time(NULL);
    gmtime_r(&n_ts, &now);

    struct tm old;
    time_t old_ts = n_ts - DEFAULT_TO_DIR_LIFE;
    gmtime_r(&old_ts, &old);

    int dir_time = (y-1900)*10000 + (m-1)*100 + d;
    int old_time = old.tm_year*10000 + old.tm_mon*100 + old.tm_mday;
    int now_time = now.tm_year*10000 + now.tm_mon*100 + now.tm_mday;
    if (dir_time <= old_time) {
        remove_dir(path);
    }
    else if (dir_time < now_time) {
        cJSON *q = files(path, NULL, NULL, NULL, 0, 0, NULL);
        if (!q || !cJSON_GetArraySize(q)) {
            remove_dir(path);
        }
        else { /* queue + q */
            if(!queue) queue = cJSON_CreateArray();
            while (cJSON_GetArraySize(q)) cJSON_AddItemToArray(queue, cJSON_DetachItemFromArray(q, 0));
        }
        if(q) cJSON_Delete(q);
    }
    return queue;
}
/*
 * Add tasks to queue. Scan all direcories on path, takes all files could be sent from yong to old.
 * Queue should be totally rewritten!
 */
static cJSON* old_all(const char* path, cJSON* queue) {
    struct dirent** list;

    if(queue) cJSON_Delete(queue);
    queue = NULL;

    int rc = scandir(path, &list, dirs_filter, alphasort);

    if(rc < 0) {
        pu_log(LL_ERROR, "%s: error scandir calling: %d - %s", __FUNCTION__, errno, strerror(errno));
        return queue;
    }
    while(rc--) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/%s", path, list[rc]->d_name);
        queue = old_dirs_delete(p, queue);
        free(list[rc]);
    }
    free(list);

    return queue;
}

typedef struct {
    char type[3];
    time_t start_date;
    time_t end_date;
} task_t;
/*
 * Converts JSON item to scan task
 * if type == "?" && !time-> scan all old
 * if type == "P" && !time -> all from SNAPSHOTS
 * if type == "?" && time -> all MD,SD,VIDEO from today
 * if type == "S" or "" or "M" && time -> given type for the period
 * else error
 * return 0 if error
 */
static int msg2type(char* msg, task_t* task) {
    int ret = 0;
    cJSON* obj = NULL;

    if(!msg) { /* Local task to scan all */
        strncpy(task->type, DEFAULT_UNDEF_FILE_POSTFIX, sizeof(task->type));
        task->start_date = 0;
        task->end_date = 0;
    }
    else {
        if(obj=cJSON_Parse(msg), !obj) {
            pu_log(LL_ERROR, "%s: error parsing %s", __FUNCTION__, msg);
            return 0;
        }
        cJSON* i;
        if(i=cJSON_GetObjectItem(obj, "type"), !i) {
            pu_log(LL_ERROR, "%s: item %s is not found in %s. Message ignored", __FUNCTION__, "type", msg);
            goto on_exit;
        }
        strncpy(task->type, i->valuestring, sizeof(task->type));
        if(i=cJSON_GetObjectItem(obj, "start_date"), !i) {
            task->start_date = 0;
        }
        else
            task->start_date = i->valueint;
        if(i=cJSON_GetObjectItem(obj, "end_date"), !i) {
            task->end_date = 0;
        }
        else
            task->end_date = i->valueint;
    }
    ret = 1;
on_exit:
    if(obj) cJSON_Delete(obj);
    return ret;
}
/*
 * If !msg -> scan for all files (MD, SD, snap, vildeo) and fill the rq
 * IF msg -> scan for fresh flies of given type
 * Queue format:
 * [<action>,...<action>]
 */
static cJSON* fill_queue(char* msg, cJSON* rq) {
    task_t task = {{0}, 0, 0};
    char buf[PATH_MAX]={0};

    if(!msg2type(msg, &task)) {
        pu_log(LL_ERROR, "%s: error parsing %s. Ignored", __FUNCTION__, msg);
        return rq;
    }
    if(!strncmp(task.type, DEFAULT_UNDEF_FILE_POSTFIX, sizeof(task.type)) && !task.start_date) {        /* if type == "?" && !time-> scan all old */
        return old_all(DEFAULT_DT_FILES_PATH, rq);
    }
    else if(!strncmp(task.type, DEFAULT_SNAP_FILE_POSTFIX, sizeof(task.type)) && !task.start_date) {    /* if type == "P" && !time -> all from SNAPSHOTS */
        snprintf(buf, sizeof(buf), "%s/%s", DEFAULT_DT_FILES_PATH, DEFAULT_SNAP_DIR);
        return files(buf, DEFAULLT_SNAP_FILE_PREFIX, DEFAULT_SNAP_FILE_POSTFIX, DEFAULT_SNAP_FILE_EXT, 0, 0, rq);
    }
    else if(!strncmp(task.type, DEFAULT_UNDEF_FILE_POSTFIX, sizeof(task.type)) && task.start_date) {    /* if type == "?" && time -> all MD,SD,VIDEO from today */
        char now_dir[20]={0};
        make_today_dir_name(now_dir, sizeof(now_dir));
        snprintf(buf, sizeof(buf), "%s/%s", DEFAULT_DT_FILES_PATH, now_dir);
        return files(buf, DEFAULT_DT_FILES_PREFIX, DEFAULT_UNDEF_FILE_POSTFIX, DEFAULT_MSD_FILE_EXT, 0,0, rq);
    }
    else if((!strncmp(task.type, DEFAULT_MD_FILE_POSTFIX, sizeof(task.type)) ||
            !strncmp(task.type, DEFAULT_SD_FILE_POSTFIX, sizeof(task.type)) ||
            !strncmp(task.type, DEFAULT_VIDEO_FILE_POSTFIX, sizeof(task.type))) && task.start_date && task.end_date) { /* if type == "S" or "" or "M" && time -> given type for the period */
        char now_dir[20]={0};
        make_today_dir_name(now_dir, sizeof(now_dir));
        snprintf(buf, sizeof(buf), "%s/%s", DEFAULT_DT_FILES_PATH, now_dir);
        return files(buf, DEFAULT_DT_FILES_PREFIX, task.type, DEFAULT_MSD_FILE_EXT, task.start_date,task.end_date, rq);
    }
    else {
        pu_log(LL_ERROR, "%s: Error task: type = %s, start_date = %lu, end_date = %lu. Ignored", __FUNCTION__, task.type, task.start_date, task.end_date);
        return rq;
    }
}
static void file_delete(const char* dir, const char* name) {
    char buf[PATH_MAX]={0};
    snprintf(buf, sizeof(buf), "%s/%s", dir, name);
    if(!unlink(buf)) {
        pu_log(LL_INFO, "%s: File %s deleted", __FUNCTION__, buf);
    }
    else {
        pu_log(LL_ERROR, "%s: error deletion %s: %d - %s", __FUNCTION__, buf, errno, strerror(errno));
    }
}
/*
 * Send files from queue if it is not empty.
 * If the file can't be sent - put it to the end of queue
 */
static cJSON* sendFromQueue(cJSON* rq) {
    if(!rq) return NULL;
    if(!cJSON_GetArraySize(rq)) {
        cJSON_Delete(rq);
        return NULL;
    }
    cJSON* item = cJSON_GetArrayItem(rq,0);
    time_t t = get_item_time(item);
    if(((unsigned int)t < UINT_MAX) && (t > time(NULL))) {
        pu_log(LL_DEBUG, "%s: 1st in queue %s/%s is to early to send", __FUNCTION__, cJSON_GetObjectItem(item, ATI_PATH)->valuestring, cJSON_GetObjectItem(item, ATI_FILE_NAME)->valuestring);
        return rq;
    }

    item = cJSON_DetachItemFromArray(rq, 0); /* Get first, remove it from array */
    fd_t task;
    int ret = json2fd(item, &task);

    if(item) cJSON_Delete(item);
    if(!ret) return rq;

    char* txt = cJSON_PrintUnformatted(rq);
    if(txt) {
        pu_log(LL_DEBUG, "%s: On entry: %s", __FUNCTION__, txt);
        free(txt);
    }

    sf_rc_t rc = send_file(&task);
    switch (rc) {
        case SF_RC_SENT_OK:
            send_alert_to_proxy(task.type[0], task.fileRef);
        case SF_RC_NO_SPACE:
        case SF_RC_BAD_FILE:
            file_delete(task.path, task.name);
            empty_dir_delete(task.path);
            break;
        case SF_RC_EARLY:
        case SF_RC_1_FAIL:
        case SF_RC_2_FAIL:
        case SF_RC_3_FAIL:
        case SF_RC_URL_TOO_OLD:
        case SF_RC_FREF_TOO_OLD: {
            cJSON *new_item = NULL;
            fd2json(&task, &new_item);                          /* Make new item with task */
			task.action = calc_action(rc);                      /* Update the action regarding the operation result */
            if(new_item) insert_task(new_item, rq);         /* Append the queue by new task */
        }
            break;
        case SF_RC_NOT_FOUND:
            /* Nothing to do */
        default:
            break;
    }
    txt = cJSON_PrintUnformatted(rq);
    if(txt) {
        pu_log(LL_DEBUG, "%s: On exit: %s", __FUNCTION__, txt);
        free(txt);
    }
    return rq;
}

static void* thread_function(void* params) {
    from_main = aq_get_gueue(AQ_ToSF);
    to_proxy = aq_get_gueue(AQ_ToProxyQueue);   /* Send alerts if the file sent */
    pu_queue_event_t events = pu_add_queue_event(pu_create_event_set(), AQ_ToSF);

    lib_timer_clock_t files_resend_clock = {0};
    lib_timer_init(&files_resend_clock, DEFAULT_TO_FOR_FILES_RESEND);

    lib_timer_clock_t send_all_clock = {0};   /* timer for backgound "send_all" task */
    lib_timer_init(&send_all_clock, DEFAULT_TO_SEND_ALL);

    lib_timer_clock_t scan_all_clock = {0};   /* timer for backgound "send_all" task */
    lib_timer_init(&scan_all_clock, DEAULT_TO_SCAN_ALL);

    cJSON* send_queue = NULL;
    cJSON* send_all_queue = NULL;
/* Check remaining files on start */
    send_queue = fill_queue(NULL, send_queue);

    while(!is_stop) {
        pu_queue_event_t ev;

        switch (ev = pu_wait_for_queues(events, 1)) {
            case AQ_ToSF: {
                size_t len = sizeof(q_msg);
                while (pu_queue_pop(from_main, q_msg, &len)) {
                    pu_log(LL_INFO, "%s: received from Agent: %s ", PT_THREAD_NAME, q_msg);
                    send_queue = fill_queue(q_msg, send_queue);
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
/* if got smth in send queue - send it */
        if(lib_timer_alarm(files_resend_clock)) {
            send_queue = sendFromQueue(send_queue);
            lib_timer_init(&files_resend_clock, DEFAULT_TO_FOR_FILES_RESEND);
        }
/* Make background "send all" if any */
        if(lib_timer_alarm(send_all_clock)) {
            send_all_queue = sendFromQueue(send_all_queue);
            lib_timer_init(&send_all_clock, DEFAULT_TO_SEND_ALL);
        }
/* Scan for old files */
        if(lib_timer_alarm(scan_all_clock)) {
            send_all_queue = fill_queue(NULL, send_all_queue);
            lib_timer_init(&scan_all_clock, DEAULT_TO_SCAN_ALL);
        }
    }
    if(send_queue) cJSON_Delete(send_queue);
    if(send_all_queue) cJSON_Delete(send_all_queue);
    return NULL;
}

int at_start_sf() {
    if(!is_stop) {
        pu_log(LL_WARNING, "%s: %s is already runs!", __FUNCTION__, PT_THREAD_NAME);
        return 1;
    }
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &thread_function, NULL)) return 0;
    return 1;

}
void at_stop_sf() {
    void *ret;
    if(is_stop) {
        pu_log(LL_WARNING, "%s: %s already stops", __FUNCTION__, PT_THREAD_NAME);
        return;
    }
    at_set_stop_sf();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}
void at_set_stop_sf() {
    is_stop = 1;
}



