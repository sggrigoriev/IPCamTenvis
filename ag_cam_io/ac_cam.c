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
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

#include <curl/curl.h>
#include <stdlib.h>

#include "cJSON.h"
#include "pu_logger.h"

#include "ag_defaults.h"
#include "au_string.h"
#include "ag_settings.h"
#include "ao_cma_cam.h"

#include "ag_db_mgr.h"
#include "ac_cam.h"

extern void sht_add(uint32_t ctx);

/***************************************************************************************************************/
/*
 * AC_CAM_STOP_MD -> DEFAULT_MD_FILE_POSTFIX
 * AC_CAM_STOP_SD -> DEFAULT_SD_FILE_POSTFIX
 * AC_CAM_MADE_SNAPSHOT -> DEFAULT_SNAP_FILE_POSTFIX
 * Anything else -> DEFAULT_UNDEF_FILE_POSTFIX
 */
const char* ac_get_event2file_type(t_ac_cam_events e) {
    const char* ret;
    switch(e) {
        case AC_CAM_STOP_MD:
            ret = DEFAULT_MD_FILE_POSTFIX;
            break;
        case AC_CAM_STOP_SD:
            ret = DEFAULT_SD_FILE_POSTFIX;
            break;
        case AC_CAM_MADE_SNAPSHOT:
            ret = DEFAULT_SNAP_FILE_POSTFIX;
            break;
        case AC_CAM_RECORD_VIDEO:
            ret = DEFAULT_VIDEO_FILE_POSTFIX;
            break;
        default:
            ret = DEFAULT_UNDEF_FILE_POSTFIX;
            pu_log(LL_ERROR, "%s: Wrong event type %d only %d, %d or %d allowed", __FUNCTION__, e, AC_CAM_STOP_MD, AC_CAM_STOP_SD, AC_CAM_MADE_SNAPSHOT);
            break;
    }
    return ret;
}
/*
 * Create name as prefixHHMMSSpostfix.ext, store it into buf
 * Return buf
 */
#define AC_CAM_FIX_L 20
const char* ac_make_name_from_date(const char* prefix, time_t timestamp, const char* postfix, const char* ext, char* buf, size_t size) {

    char fix[AC_CAM_FIX_L] = {0};
    if(!ext || !strlen(ext)) {
        pu_log(LL_ERROR, "%s no extention found! No Masha - no candy", __FUNCTION__);
        return "";
    }
    size_t tot_len = (prefix)?strlen(prefix):0 + AC_CAM_FIX_L + (postfix)?strlen(postfix):0 + strlen(ext)+1;
    if(size <= tot_len) {
        pu_log(LL_ERROR, "%s: buffer size too small. %d size requires", __FUNCTION__, tot_len+1);
        return "";
    }
    struct tm tm_d;
    gmtime_r(&timestamp, &tm_d);
    snprintf(fix, sizeof(fix)-1, "%02d%02d%02d", tm_d.tm_hour, tm_d.tm_min, tm_d.tm_sec);
    snprintf(buf, size, "%s%s%s.%s", (prefix)?prefix:"", fix, (postfix)?postfix:"", ext);
    return buf;

}
/*
 * Makes path directory. If already exists - ok all other errors reported!
 * called from cam_init
 */
void ac_make_directory(const char* path, const char* dir_name) {
    char buf[256] = {0};
    if((!path) || (!dir_name)) {
        pu_log(LL_ERROR, "%s path or dir_name is NULL. Can't work in such nervious conditions!", __FUNCTION__);
        return;
    }
    snprintf(buf, sizeof(buf)-1, "%s/%s", path, dir_name);

    int ret = mkdir(buf, 0777);
    if(!ret || (errno == EEXIST)) return;    /* Directory was created or was alreay created - both cases are suitable */

    pu_log(LL_ERROR, "%s: error directory %s creattion %d - %s", __FUNCTION__, buf, errno, strerror(errno));
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
    if(res = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_USERNAME, ag_getCamLogin()), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(curl, CURLOPT_PASSWORD, ag_getCamPassword()), res != CURLE_OK) goto on_error;
/*    if(res = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, 0L), res != CURLE_OK) goto on_error; */
    return curl;
on_error:
    curl_easy_cleanup(curl);
    return NULL;
}
static void close_curl_session(CURL* curl) {
    if(curl) curl_easy_cleanup(curl);
}
static int send_command(const char* url_cmd, const char* params) {
    int ret = 0;
    CURLcode res;

    CURL* crl = open_curl_session();
    if(!crl) return 0;

    FILE* fpw=NULL;
    char* ptrw=NULL;
    size_t szw=0;
    struct curl_slist *hs=NULL;

    fpw = open_memstream(&ptrw, &szw);
    if( fpw == NULL ) {
        pu_log(LL_ERROR, "%s: Error open file: %d - %s\n", __FUNCTION__, errno, strerror(errno));
        goto on_error;
    }

    if(hs = curl_slist_append(hs, "Content-Type: text/plain;charset=UTF-8"), !hs) goto on_error;

    if(res = curl_easy_setopt(crl, CURLOPT_HTTPHEADER, hs), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_URL, url_cmd), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_POSTFIELDS, params), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_POST, 1L), res != CURLE_OK) goto on_error;

    if(res = curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, NULL), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_WRITEDATA, fpw), res != CURLE_OK) goto on_error;

    if(res = curl_easy_perform(crl), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Curl error %s", __FUNCTION__, curl_easy_strerror(res));
        goto on_error;
    }

    fflush(fpw);
    if(ptrw && strlen(ptrw)) {
        pu_log(LL_DEBUG, "%s: returned %s", __FUNCTION__, ptrw);
    }

    ret = 1;
on_error:
    close_curl_session(crl);
    if(hs) curl_slist_free_all(hs);
    if(fpw) fclose(fpw);
    if(ptrw) free(ptrw);

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

    if(res = curl_easy_perform(crl), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Curl error %s", __FUNCTION__, curl_easy_strerror(res));
        goto on_error;
    }

    fflush(fp);
    if(!ptr) {
        pu_log(LL_ERROR, "%s: Cam returns NULL answer on %s request", __FUNCTION__, url_cmd);
        goto on_error;
    }
    ret = calloc(sz+1, 1);
    memcpy(ret, ptr, sz);
    ret[sz] = '\0';   /* to be sure about NULL-termination */
on_error:
    close_curl_session(crl);
    if(fp)fclose(fp);
    if(ptr)free(ptr);
    return ret;
}
/*
 * Read params, set par_id to par_value, set updated params, read, extract par_id and return it's new value
 */
static int update_one_parameter(int cmd_id, user_par_t par_id, int par_value) {
    int ret = 0;
    char* lst=NULL;
    char* read_uri=0;
    char* write_uri=0;
/*Read params */
    if(read_uri = ao_make_cam_uri(cmd_id, AO_CAM_READ), !read_uri) goto on_error;
    if(lst = get_current_params(read_uri), !lst) goto on_error;
    pu_log(LL_DEBUG, "%s: for %d par_id = %d, new val = %d. Current params for %d =\n%s", __FUNCTION__, cmd_id, par_id, par_value, cmd_id, lst);
    ao_save_params(cmd_id, lst);
    free(lst);
/* Update local store by our parameter */
    ao_save_parameter(cmd_id, par_id, par_value);

/*Prepare new list */
    if(lst = ao_make_params(cmd_id), !lst) goto on_error;
    pu_log(LL_DEBUG, "%s: Update list for %d =\n%s", __FUNCTION__, cmd_id, lst);
/*Write updated params list */
    if(write_uri = ao_make_cam_uri(cmd_id, AO_CAM_WRITE), !write_uri) goto on_error;
    if(!send_command(write_uri, lst)) goto on_error;
    free(lst);
/* Re-read cam params. */
    if(lst = get_current_params(read_uri), !lst) goto on_error;
    pu_log(LL_DEBUG, "%s: New params for %d =\n%s", __FUNCTION__, cmd_id, lst);

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
    const char* TIME_1st_PARAM = 
            "time=";    
    const char* TIME_FORMAT = 
            "%Y%m%d%H%M%S";
    const char* TIME_REST_PARAMS = 
            "&ntpen=1&tz=GMT&location=GMT&ck_dst=0&ntpserver=time.nist.gov";

    const char* H264_INIT_PARAMS =
            "encoder=0&res=8&fmode=0&bps=2048&fps=25&gop=50&quality=0&chn=0&sub=0&res_mask 256&max_fps=30";
    
    const char* CFGREC_INIT_PARAMS =
            "-sizelmt 100\n-timelmt 120\n-alrmtrgrec 20\n-vstrm 0\n-record_audio 1\n-snap_instead 0\n-snap_interval 60"
            "\n-schedule \"default\"\nrecchn 3\n\n\n";
    
    const char* SETVIDEO_INIT_PARAMS =
            "active=1&norm=1&achn=0&res_comb=0&res=8&fmode=0&bps=2048&fps=25&gap=30&quality=0"
            "&res_sub=3&fmode_sub=0&bps_sub=308&fps_sub=25&gap_sub=30&quality_sub=0"
            "&res_rec=8&fmode_rec=0&bps_rec=2048&fps_rec=25&gap_rec=30&quality_rec=0&chn=0";

    const char* MD_INIT_PARAMS = 
            "recch=1&tapech=1&ts0=&ts1=&ts2=&ts3=&dealmode=536870912&rect0=0,0,999,999, 5&rect1=&rect2=&rect3=&chn=0";
    
    const char* SD_INIT_PARAMS = 
            "tapech=1&recch=1&ts0=&ts1=&ts2=&ts3=&dealmode=536870912&enable=1&sensitivity=6";

    char* h264_uri = NULL;
    char* cfgrec_uri = NULL;
    char* setvideo_uri = NULL;
    char* md_uri = NULL;
    char* sd_uri = NULL;
    char* time_uri = NULL;
    int ret = 0;
/*
    if(h264_uri = ao_make_cam_uri(AO_CAM_CMD_H264, AO_CAM_WRITE), !h264_uri) goto on_error;
    if(!send_command(h264_uri, H264_INIT_PARAMS)) pu_log(LL_ERROR, "%s: Error h264 initiation", __FUNCTION__);
*/
    if(cfgrec_uri = ao_make_cam_uri(AO_CAM_CMD_CFGREC, AO_CAM_WRITE), !cfgrec_uri) goto on_error;
    if(!send_command(cfgrec_uri, CFGREC_INIT_PARAMS)) pu_log(LL_ERROR, "%s: Error cfgrec initiation", __FUNCTION__);
/*
    if(setvideo_uri = ao_make_cam_uri(AO_CAM_CMD_SETVIDEO, AO_CAM_WRITE), !setvideo_uri) goto on_error;
    if(!send_command(setvideo_uri, SETVIDEO_INIT_PARAMS)) pu_log(LL_ERROR, "%s: Error setvideo initiation", __FUNCTION__);
*/
    if(md_uri = ao_make_cam_uri(AO_CAM_CMD_MD, AO_CAM_WRITE), !md_uri) goto on_error;
    if(!send_command(md_uri, MD_INIT_PARAMS)) pu_log(LL_ERROR, "%s: Error MD initiation", __FUNCTION__);

    if(sd_uri = ao_make_cam_uri(AO_CAM_CMD_SD, AO_CAM_WRITE), !sd_uri) goto on_error;
    if(!send_command(sd_uri, SD_INIT_PARAMS)) pu_log(LL_ERROR, "%s: Error SD initiation", __FUNCTION__);

    struct tm t;
    char dat[15] = {0};
    char buf[256]={0};
    time_t timestamp = time(NULL);
    gmtime_r(&timestamp, &t);
    strftime(dat, sizeof(dat), TIME_FORMAT, &t);
    snprintf(buf, sizeof(buf)-1, "%s%s%s", TIME_1st_PARAM, dat, TIME_REST_PARAMS);

    if(time_uri = ao_make_cam_uri(AO_CAM_CMD_TIME, AO_CAM_WRITE), !time_uri) goto on_error;
    if(!send_command(time_uri, buf)) pu_log(LL_ERROR, "%s: Error Camera time setup", __FUNCTION__);
    
    pu_log(LL_INFO, "%s: Initiation parameters for H264 %s", __FUNCTION__, H264_INIT_PARAMS);    
    pu_log(LL_INFO, "%s: Initiation parameters for CFGREC %s", __FUNCTION__, CFGREC_INIT_PARAMS);
    pu_log(LL_INFO, "%s: Initiation parameters for SETVIDEO %s", __FUNCTION__, SETVIDEO_INIT_PARAMS);
    pu_log(LL_INFO, "%s: Initiation parameters for MD %s", __FUNCTION__, MD_INIT_PARAMS);
    pu_log(LL_INFO, "%s: Initiation parameters for SD %s", __FUNCTION__, SD_INIT_PARAMS);
    pu_log(LL_INFO, "%s: Initiation parameters for TIME %s", __FUNCTION__, buf);
/* TODO if can't make directory - snaphots sohuld be disabled! */
    ac_make_directory(DEFAULT_DT_FILES_PATH, DEFAULT_SNAP_DIR);

    ret = 1;
on_error:
    if(h264_uri) free(h264_uri);
    if(cfgrec_uri) free(cfgrec_uri);
    if(setvideo_uri) free(setvideo_uri);
    if(md_uri) free(md_uri);
    if(sd_uri) free(sd_uri);
    if(time_uri) free(time_uri);
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
    IP_CTX_(300);
    int ret = 0;
    CURLcode res;
    FILE* fp=NULL;
    char* uri = NULL;
    CURL* crl = NULL;

    if(crl = open_curl_session(), !crl) return 0;

    if(fp = fopen(full_path, "wb"), !fp) {
        pu_log(LL_ERROR, "%s: Error open file: %d - %s\n", __FUNCTION__, errno, strerror(errno));
        goto on_error;
    }

    if(uri = ao_make_cam_uri(AO_CAM_CMD_SNAPSHOT, AO_CAM_WRITE), !uri) goto on_error;

    if(res = curl_easy_setopt(crl, CURLOPT_URL, uri), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, NULL), res != CURLE_OK) goto on_error;
    if(res = curl_easy_setopt(crl, CURLOPT_WRITEDATA, fp), res != CURLE_OK) goto on_error;

    if(res = curl_easy_perform(crl), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Curl error %s", __FUNCTION__, curl_easy_strerror(res));
        goto on_error;
    }
    pu_log(LL_INFO, "%s: Got the picture!\n", __FUNCTION__);
    ret = 1;
on_error:
    close_curl_session(crl);
    if(fp)fclose(fp);
    if(uri) free(uri);
    IP_CTX_(301);
    return ret;
}
int ac_cam_make_video() {
    int ret = 0;
    CURLcode res;
    FILE* fpw=NULL;
    char* ptrw=NULL;
    size_t szw=0;
    const char* uri;

    CURL* crl = open_curl_session();
    if(!crl) return 0;

    fpw = open_memstream(&ptrw, &szw);
    if( fpw == NULL ) {
        pu_log(LL_ERROR, "%s: Error open file: %d - %s\n", __FUNCTION__, errno, strerror(errno));
        goto on_error;
    }
    if(uri = ao_make_cam_uri(AO_CAM_CMD_CAPTURE_VIDEO, AO_CAM_WRITE), !uri) goto on_error;

    curl_easy_setopt(crl, CURLOPT_URL, uri);
    curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(crl, CURLOPT_WRITEDATA, fpw);
    res = curl_easy_perform(crl);
    if(res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: %s\n", __FUNCTION__, curl_easy_strerror(res));
        goto on_error;
    }
    fflush(fpw);

    ret = 1;
on_error:
    close_curl_session(crl);
    if(fpw) fclose(fpw);
    if(ptrw) free(ptrw);
    return ret;
}
/*
 * on could be -1,0,1,2
 */
int ac_set_md(int on) {
    pu_log(LL_DEBUG, "%s: on entry. On = %d", __FUNCTION__, on);
    int new_val = update_one_parameter(AO_CAM_CMD_MD, AO_CAM_PAR_MD_ON, (on > 0)?1:0);
    switch (on) {
        case -1:
            if(!new_val) return -1;
            else return 2;
            return new_val;
        case 2:
            if(new_val) return 2;
            else return -1;
            break;
        case 0:
        case 1:
        default:
            break;
    }
    pu_log(LL_DEBUG, "%s: on exit. new_val = %d", __FUNCTION__, new_val);
    return new_val;
}
int ac_set_sd(int on) {
    int new_val = update_one_parameter(AO_CAM_CMD_SD, AO_CAM_PAR_SD_ON, on);
    switch (on) {
        case -1:
            if(!new_val) return -1;
            else return 2;
            return new_val;
        case 2:
            if(new_val) return 2;
            else return -1;
            break;
        case 0:
        case 1:
        default:
            break;
    }
    return new_val;
}
int ac_set_audio(int on) {
    return update_one_parameter(AO_CAM_CMD_CFGREC, AO_CAM_PAR_CFGREC_AUDIO_ON, on);
}

/*
 * Set new value: read, update, re-read and return back
 * IMDB methods (IMDB.cammethod field
 */
int ac_set_sd_sensitivity(int value) {
    return update_one_parameter(AO_CAM_CMD_SD, AO_CAM_PAR_SD_SENS, value);
}
int ac_set_md_sensitivity(int value) {
    return update_one_parameter(AO_CAM_CMD_MD, AO_CAM_PAR_MD_SENS, value);
}
