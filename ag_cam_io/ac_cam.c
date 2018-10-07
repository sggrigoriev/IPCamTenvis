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
 Created by gsg on 25/02/18.
*/

#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>

#include <curl/curl.h>

#include "cJSON.h"
#include "pu_logger.h"

#include "ag_settings.h"
#include "ao_cma_cam.h"
#include "ac_http.h"
#include "ag_db_mgr.h"
#include "ac_cam.h"

const char* make_dir_from_time(const char* path, time_t timestamp, char* buf, size_t size) {
    struct tm t;
    char name[11] = {0};

    gmtime_r(&timestamp, &t);
    strftime(name, sizeof(name), "%Y-%m-%d", &t);
    strncpy(buf, path, size-1);
    strncat(buf, "/", size-2);
    strncat(buf, name, size - strlen(buf)-1);
    return buf;
}
/*
 * Return 1 if the file name is preffix%%%%%%postfix...
 */
static int is_right_name(const char* name, const char*prefix, const char* postfix) {
    if(strlen(name)<(strlen(prefix)+strlen(postfix)+6)) return 0;
    if(strncmp(name, prefix, strlen(prefix)) != 0) return 0;
    if(strncmp(name+strlen(prefix)+6, postfix, strlen(postfix)) != 0) return 0;
    return 1;
}
/*
 * convert hrs, mins, seconds from struct tm to the number hhmmss
 */
static unsigned long tm2dig(struct tm t) {
    return (unsigned long)t.tm_sec+(unsigned long)t.tm_min*100+(unsigned long)t.tm_hour*10000;
}
/*
 * return 1 if str conferted as hhmmss to long is between start and end
 */
static int is_between(unsigned long start, const char* str, unsigned long end) {
    int h, m, s;

    int i = sscanf(str, "%2i%2i%2i", &h, &m, &s);
    if(i != 3) {
        printf("Error name scan: %d %s\n", errno, strerror(errno));
        return 0;
    }
    unsigned long md = (unsigned long)s+(unsigned long)m*100+(unsigned long)h*10000;

    return ((start <= md) && (md < end));
}
/* concatinate [name, name, ... name] */
char* add_files_list(const char* dir_name, time_t start, time_t end, const char* postfix, char* buf, size_t size) {
    DIR *dir = opendir(dir_name);
    buf[0] = '\0';
    if (dir == NULL)        /* Not a directory or doesn't exist */
        return buf;
    else {
        struct dirent* dir_ent;
        int first = 0;
        while((dir_ent = readdir(dir)), dir_ent != NULL) {
            struct tm tm_s, tm_e;
            gmtime_r(&start, &tm_s);
            gmtime_r(&end, &tm_e);
            if(is_right_name(dir_ent->d_name, DEFAULT_DT_FILES_PREFIX, postfix) && is_between(tm2dig(tm_s), dir_ent->d_name+strlen(DEFAULT_DT_FILES_PREFIX), tm2dig(tm_e))) {
                if(!first) {
                    first = 1;
                    strncat(buf, "[", size - strlen(buf)-1);
                }
                strncat(buf, dir_ent->d_name, size - strlen(buf)-1);
                strncat(buf, ", ", size - strlen(buf)-1);
            }
        }
        if(strlen(buf)) buf[strlen(buf)-2] = ']';  /* Replace last ',' to ']'*/
        closedir(dir);
    }
    return buf;
}
/***************************************************************************************************************/

/*
 * Create the JSON array with full file names& path for alert "filesList":["name1",..."nameN"]
 * If no files found - return empty string
 * 1. Find directory "yyyy-mm-dd" in DEFAULT_MD_FILES_PATH
 * 2. find files (S or M type) with filename as DEFAULT_ХХ_FILES_PREFIX+hhmmss+DEFAULT_XX_FILE_POSTFIX.*
 *      where hhmmss is between start and end dates of the alert
 */
const char* ac_cam_get_files_name(t_ao_cam_alert data, char* buf, size_t size) {
    char dir[256] = {0};
    const char* postfix;

    make_dir_from_time(DEFAULT_DT_FILES_PATH, data.start_date, dir, sizeof(dir));
    if(data.cam_event == AC_CAM_STOP_MD) postfix = DEFAULT_MD_FILE_POSTFIX;
    else if(data.cam_event == AC_CAM_STOP_MD) postfix = DEFAULT_SD_FILE_POSTFIX;
    else {
        pu_log(LL_ERROR, "%s: Wrong event %s only %s or %s expected", __FUNCTION__, ac_cam_event2string(data.cam_event), ac_cam_event2string(AC_CAM_STOP_MD), ac_cam_event2string(AC_CAM_STOP_SD));
        buf[0] = '\0';
        return buf;
    }
    strncpy(buf, "filesList: ", size-1);
    size_t len = strlen(buf);
    add_files_list(dir, data.start_date, data.end_date, postfix, buf, size);
    if(strlen(buf) <= len) {
        pu_log(LL_WARNING, "%s: no files were found", __FUNCTION__);
        buf[0] = '\0';
    }
    return buf;
}

/***************************
 * cURL support local functions
 */
static CURL *open_curl_session(){
    CURL* curl;
    CURLcode res;
    curl = curl_easy_init();
    if(!curl) {
        pu_log(LL_ERROR, "%s: Error on curl_easy_init call.", __FUNCTION__);
        return NULL;
    }
    if(res = curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC), res != CURLE_OK) goto on_error;
    if (res = curl_easy_setopt(curl, CURLOPT_USERNAME, ag_getCamLogin()), res != CURLE_OK) goto on_error;
    if (res = curl_easy_setopt(curl, CURLOPT_PASSWORD, ag_getCamPassword()), res != CURLE_OK) goto on_error;
    if (res = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, 0L), res != CURLE_OK) goto on_error;
    return curl;
on_error:
    curl_easy_cleanup(curl);
    return NULL;
}
static void close_curl_session(CURL* curl) {
    curl_easy_cleanup(curl);
}
static int send_command(const char* url_cmd, const char* params) {
    int ret = 0;
    CURLcode res;

    CURL* crl = open_curl_session();
    if(!crl) return 0;

    struct curl_slist *hs=NULL;
    if(hs = curl_slist_append(hs, "Content-Type: text/plain;charset=UTF-8"), !hs) goto on_error;

    if(res = curl_easy_setopt(crl, CURLOPT_HTTPHEADER, hs), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_URL, url_cmd), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_POSTFIELDS, params), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_POST, 1L), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(crl);
    if(ac_http_analyze_perform(res, crl, __FUNCTION__) != CURLE_OK) goto on_error;

    ret = 1;
on_error:
    if(hs) curl_slist_free_all(hs);
    close_curl_session(crl);
    return ret;
}
/* Return name value\n...name value\n string. */
static char* get_current_params(const char* url_cmd) {
    char* ret = NULL;

    CURL* crl = open_curl_session();
    if(!crl) return NULL;

    CURLcode res;
    FILE* fp = NULL;
    char* ptr = NULL;
    size_t sz=0;

    if(fp = open_memstream(&ptr, &sz), !fp) {
        pu_log(LL_ERROR, "%s: Error open memstream: %d - %s\n", __FUNCTION__, errno, strerror(errno));
        goto on_error;
    }

    if(res = curl_easy_setopt(crl, CURLOPT_URL, url_cmd), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, NULL), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_WRITEDATA, fp), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(crl);
    if (ac_http_analyze_perform(res, crl, __FUNCTION__) != CURLE_OK) goto on_error;

    fflush(fp);
    if(!ptr) {
        pu_log(LL_ERROR, "%s: Cam returns empty answer on %s request", __FUNCTION__, url_cmd);
        goto on_error;
    }
    ret = calloc(1, sz+1);
    memcpy(ret, ptr, sz);
    ret[sz] = '\0';   /* to be sure about NULL-termination */
on_error:
    fclose(fp);
    free(ptr);
    close_curl_session(crl);
    return ret;
}
/*
 * Read params, set par_id to par_value, set updated params, read, extract par_id and return it's new value
 */
static int update_one_parameter(int cmd_id, user_par_t par_id, int par_value) {
    int ret = 0;
    char* lst=0;
    char* read_uri=0;
    char* write_uri=0;
/*Read params */
    if(read_uri = ao_make_cam_uri(cmd_id, AO_CAM_READ), !read_uri) goto on_error;
    if(lst = get_current_params(read_uri), !lst) goto on_error;
    pu_log(LL_DEBUG, "%s: Current params for %d = %s", __FUNCTION__, cmd_id, lst);
    ao_save_params(cmd_id, lst);
    free(lst);
/* Update local store by our parameter */
    ao_save_parameter(cmd_id, par_id, par_value);

/*Prepare new list */
    if(lst = ao_make_params(cmd_id), !lst) goto on_error;
    pu_log(LL_DEBUG, "%s: Update list for %d = %s", __FUNCTION__, cmd_id, lst);
/*Write updated params list */
    if(write_uri = ao_make_cam_uri(cmd_id, AO_CAM_WRITE), !write_uri) goto on_error;
    if(!send_command(write_uri, lst)) goto on_error;
    free(lst);
/* Re-read cam params. */
    if(lst = get_current_params(read_uri), !lst) goto on_error;
    pu_log(LL_DEBUG, "%s: New params for %d = %s", __FUNCTION__, cmd_id, lst);

    ao_save_params(cmd_id, lst);
    free(lst); lst = NULL;
    ret = ao_get_param_value(cmd_id, par_id);

on_error:
    if(read_uri) free(read_uri);
    if(write_uri) free(write_uri);
    if(lst) free(lst);
    return ret;
}

/*
 * Some initial cam activites...
 * Set MD as
 * "recch=1&tapech=1&dealmode=0x20000001&rect0=0,0,999,999,6&$rect1=&rect2=&rect3=&chn=0" -- w/o any TS
 * Set SD as
 * "enable=1&sensitivity=5&tapech=1&recch=1&dealmode=0x20000001&chn=0" -- w/o any TS
 */
int ac_cam_init() {
    const char* MD_INIT_PARAMS = "recch=1&tapech=1&ts0=&ts1=&ts2=&ts3=&dealmode=536870912&rect0=0,0,999,999, 5&rect1=&rect2=&rect3=&chn=0";
    const char* SD_INIT_PARAMS = "tapech=1&recch=1&ts0=&ts1=&ts2=&ts3=&dealmode=536870912&enable=1&sensitivity=6";

    char* md_uri = NULL;
    char* sd_uri = NULL;
    int ret = 0;
    if(md_uri = ao_make_cam_uri(AO_CAM_CMD_MD, AO_CAM_WRITE), !md_uri) goto on_error;
    if(!send_command(md_uri, MD_INIT_PARAMS)) pu_log(LL_ERROR, "%s: Error MD initiation", __FUNCTION__);

    if(sd_uri = ao_make_cam_uri(AO_CAM_CMD_SD, AO_CAM_WRITE), !sd_uri) goto on_error;
    if(!send_command(sd_uri, SD_INIT_PARAMS)) pu_log(LL_ERROR, "%s: Error SD initiation", __FUNCTION__);

    ret = 1;
    on_error:
    if(md_uri) free(md_uri);
    if(sd_uri) free(sd_uri);
    return ret;
}
void ac_cam_deinit() {

}
/*
 * Make picture and store it by full_path
 * Return 0 if error
 * Return 1 if OK
 */
int ac_cam_make_snapshot(const char* full_path) {
    int ret = 0;
    CURLcode res;
    FILE* fp;

    CURL* crl = open_curl_session();
    if(!crl) return 0;

    if(fp = fopen(full_path, "wb"), !fp) {
        pu_log(LL_ERROR, "%s: Error open file: %d - %s\n", __FUNCTION__, errno, strerror(errno));
        goto on_error;
    }
    char* uri = ao_make_cam_uri(AO_CAM_CMD_SNAPSHOT, AO_CAM_WRITE);
    if(!uri) goto on_error;

    if(res = curl_easy_setopt(crl, CURLOPT_URL, uri), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, NULL), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_WRITEDATA, fp), res != CURLE_OK) goto on_error;

    res = curl_easy_perform(crl);
    if(ac_http_analyze_perform(res, crl, __FUNCTION__) != CURLE_OK) goto on_error;

    pu_log(LL_INFO, "%s: Got the picture!\n", __FUNCTION__);
    ret = 1;
on_error:
    fclose(fp);
    close_curl_session(crl);
    return ret;
}
int ac_set_md(int on) {
    return update_one_parameter(AO_CAM_CMD_MD, AO_CAM_PAR_MD_ON, on);
}
int ac_set_sd(int on) {
    return update_one_parameter(AO_CAM_CMD_SD, AO_CAM_PAR_SD_ON, on);
}

/*
 * Set new value: read, update, re-read and return back
 */
int ac_set_sd_sensitivity(int value) {
    return update_one_parameter(AO_CAM_CMD_SD, AO_CAM_PAR_SD_SENS, value);
}
int ac_set_md_sensitivity(int value) {
    return update_one_parameter(AO_CAM_CMD_MD, AO_CAM_PAR_MD_SENS, value);
}
