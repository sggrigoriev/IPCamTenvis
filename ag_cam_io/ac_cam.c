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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <curl/curl.h>

#include "cJSON.h"
#include "pu_logger.h"

#include "ag_defaults.h"
#include "au_string.h"
#include "ag_settings.h"
#include "ao_cma_cam.h"

#include "ag_db_mgr.h"
#include "ac_cam.h"

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
static const char* get_sfull_name(const char* path, const char* name, char* dir_buf) {
    snprintf(dir_buf, PATH_MAX, "%s/%s", path, name);
    return dir_buf;
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
        free(full_name);
    }
    closedir(d);

    return rmdir(path_n_name);
}

static const char* add_dir_from_date(time_t timestamp, char* buf, size_t size) {
    struct tm t;
    char name[11] = {0};

    gmtime_r(&timestamp, &t);
    strftime(name, sizeof(name), DEFAULT_DT_DIRS_FMT, &t);
    strncat(buf, name, size-1);
    buf[size-1] = '\0';
    return buf;
}
/*
 * return 1 if the name is YYYY-MM-DD
 * if old == 1 -> the date represented by name sohuld be less than today
 */
static int is_right_dir(const char* name, int old) {
    if((!name) && (strlen(name)!=strlen("YYYY-MM-DD"))) return 0;
    if(old) {
        char dir[20]={0};
        add_dir_from_date(time(NULL), dir, sizeof(dir));
        if(!strcmp(name, dir)) return 0;
    }
    int f, y, m, d;
    char c1, c2;
    f = sscanf(name, "%4d%[-]%2d%[-]%2d", &y, &c1, &m, &c2, &d);
    if(f != 5) return 0;
    return 1;
}
/*
 * check if t between start_date and end_date
 */
static int in_dates(time_t t, time_t start_date, time_t end_date) {
    if(!start_date && !end_date) return 1;
    if(!start_date) return t <= end_date;
    if(!end_date) return t >= start_date;

    return ((start_date <= t) && (t <= end_date));
}
/*
 * Convert year, month, day, hour, minute, second to time_t
 */
static time_t make_date(int y, int m, int d, int hr, int min, int sec) {
    y = (y)?y-1900:1900;
    m = (m)?m-1:0;
    struct tm time_struct = {sec, min, hr, d, m, y, 0, 0, 0};
    return mktime(&time_struct);
}

static int useful_file(const struct dirent* ent, const char* postfix, time_t start_date, time_t end_date) {
    if(ent->d_type != DT_REG) return 0;
    if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) return 0;

    char pr[10]={0},t[10]={0},dot[10]={0},ext[10]={0};
    int h,m,s;

    int f = sscanf(ent->d_name, "%2s%2i%2i%2i%1s%1s%s", pr, &h, &m, &s, t, dot, ext);
    if(f != 7) return 0;

    pu_log(LL_DEBUG, "%s: File name is %s, parsed name is =%s=%i=%i=%i=%s=%s=%s", __FUNCTION__, ent->d_name, pr,h,m,s,t,dot,ext);

    if(strcmp(pr, DEFAULT_DT_FILES_PREFIX) != 0) return 0;
    if(strcmp(t, postfix) != 0) return 0;

    return in_dates(make_date(0,0,0,h,m,s), start_date, end_date);
}
/*
 * Return 1 if we need files from the directory
 */
static int useful_dir(const struct dirent* ent, const char* postfix, time_t start_date, time_t end_date) {
    if (!ent) return 0;
    if (ent->d_type != DT_DIR) return 0;
    if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) return 0;
    if (!strcmp(ent->d_name, DEFAULT_SNAP_DIR) && !strcmp(postfix, DEFAULT_SNAP_FILE_POSTFIX)) return 1;

    if(strlen(ent->d_name)!=strlen("YYYY-MM-DD")) return 0;
    if(strcmp(postfix, DEFAULT_MD_FILE_POSTFIX) && strcmp(postfix, DEFAULT_SD_FILE_POSTFIX)) return 0;
    int f, y, m, d;
    char c1, c2;
    f = sscanf(ent->d_name, "%4d%[-]%2d%[-]%2d", &y, &c1, &m, &c2, &d);
    if(f != 5) return 0;

    return in_dates(make_date(y,m,d,0,0,0), start_date, end_date);
}
/*
 * add ,"fnmame" to the fl->list if list length will be less than tot_len
 * return 1 of OK, return 0 if name wasn't added
 */
static int add_name_to_list(char** list, const char* fname, size_t tot_len) {
    size_t list_len = (*list)?strlen(*list):0;
    if((list_len+strlen(fname)+3) >= tot_len) return 0; /* We can't add ',"fname"' */

    char buf[512] = {0};
    snprintf(buf, sizeof(buf), "\",%s,\"", fname);

    *list = au_append_str(*list, buf);
    return *list != NULL;
}
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
 * Return 0 if error or nothing to send
 */
int ac_cam_fl_open(ac_cam_fl_t* fl, const char* postfix, time_t start_date, time_t end_date) {
    int ret = 0;
    fl->postfix = postfix;
    fl->start_date = start_date;
    fl->end_date = end_date;
    fl->root = NULL;
    fl->root_ent = NULL;
    fl->files_stor = NULL;
    fl->files_stor_ent = NULL;
    fl->list = NULL;

    if(fl->root = opendir(DEFAULT_DT_FILES_PATH), !fl->root) { /* Not a directory or doesn't exist */
        pu_log(LL_ERROR, "%s: Directory %s wasn't found", __FUNCTION__, DEFAULT_DT_FILES_PATH);
        goto on_exit;
    }
    ret = 1;
on_exit:
    return ret;
}
/*
 * return "name", ..., "name" less than size bytes or NULL if no more data
 */
const char* ac_cam_fl_get_next(ac_cam_fl_t* fl, size_t size) {
    char* ret = NULL;

    if(fl->list) {
        free(fl->list);
        fl->list = NULL;
    }
    if(fl->root_ent == NULL) goto on_start;
    goto on_entry;
on_start:
    while (fl->root_ent = readdir(fl->root), fl->root != NULL) {
        if(!useful_dir(fl->root_ent, fl->start_date, fl->end_date)) continue;
        if(fl->files_stor = opendir(get_sfull_name(DEFAULT_DT_FILES_PATH, fl->root_ent->d_name, fl->dir_name)), !fl->files_stor) goto on_exit;

        while (fl->files_stor_ent = readdir(fl->files_stor), fl->files_stor != NULL) {

            if(!useful_file(fl->files_stor_ent, fl->postfix, fl->start_date, fl->end_date)) continue;
on_entry:   if(!add_name_to_list(&fl->list, get_sfull_name(fl->dir_name, fl->files_stor_ent->d_name, fl->file_name), size)) {   /* Size is over */
                ret = fl->list;
                goto on_exit;
            }
        }
        closedir(fl->files_stor);
        fl->files_stor_ent = NULL;
        fl->files_stor = NULL;
    }
    fl->root_ent = NULL;
    return NULL;
on_exit:
    if(!fl->files_stor) closedir(fl->files_stor);
    return ret;
}
void ac_cam_fl_close(ac_cam_fl_t* fl) {
    if(fl->root) closedir(fl->root);
    fl->root = NULL;
    if(fl->list) free(fl->list);
    fl->list = NULL;
}
/*
 * Return empty string or all shit after the first '.' in file name
 */
const char* ac_cam_get_file_ext(const char* name) {
    const char* ret = strchr(name, '.');
    return (!ret)?"":ret+1;
}
/*
 * Return file size in bytes
 */
unsigned long ac_get_file_size(const char* name) {
    struct stat st;
    if(lstat(name, &st)) {
        pu_log(LL_ERROR, "%s: lstat %s error: %d - %s", __FUNCTION__, name, errno, strerror(errno));
        return 0;
    }
    return st.st_size;
}
/*
 * Delete all files from directory
 */
void ac_cam_clean_dir(const char* path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {       /* Not a directory or doesn't exist */
        pu_log(LL_WARNING, "%s: Directory %s wasn't found", __FUNCTION__, path);
        return;
    }
    struct dirent* dir_ent;
    while((dir_ent = readdir(dir)), dir_ent != NULL) {
        if(dir_ent->d_type != DT_REG) continue;

        char buf[512] = {0};
        snprintf(buf, sizeof(buf)-1, "\"%s/%s\",", path, dir_ent->d_name);
        if(!unlink(buf))
            pu_log(LL_DEBUG, "%s: file %s deleted", __FUNCTION__, buf);
        else
            pu_log(LL_ERROR, "%s: file %s delete error %d - %s", __FUNCTION__, buf, errno, strerror(errno));
    }
    closedir(dir);
}
/*
 * Delete all directories which are empty and elder than today
 * Called from ac_cam_init()
 */
void ac_delete_old_dirs(time_t life_time) {
    DIR *dir = opendir(DEFAULT_DT_FILES_PATH);
    if (dir == NULL) {       /* Not a directory or doesn't exist */
        pu_log(LL_WARNING, "%s: Directory %s wasn't found", __FUNCTION__, DEFAULT_DT_FILES_PATH);
        return;
    }
    struct dirent *dir_ent;
    time_t end_date = time(NULL)-life_time;
    while (dir_ent = readdir(dir), dir_ent != NULL) {
        if(!useful_dir(dir_ent, DEFAULT_MD_FILE_POSTFIX, 0, end_date)) continue; /* Doesn't matter SD or MD preffix to use here */
        if(remove_dir(dir_ent->d_name) || errno == ENOENT) {
            pu_log(LL_DEBUG, "%s: Directory %s was deleted", __FUNCTION__, dir_ent->d_name);
        }
        else {
            pu_log(LL_ERROR, "%s: Error %s deletion: %d - %s", __FUNCTION__, dir_ent->d_name, errno, strerror(errno));
        }
    }
    closedir(dir);
}
/*
 * Create name as prefixYYYY-MM-DD_HHMMSSpostfix.ext, store it into buf
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
    snprintf(fix, sizeof(fix)-1, "%04d-%02d-%02d_%02d%02d%02d", tm_d.tm_year+1900, tm_d.tm_mon+1, tm_d.tm_mday, tm_d.tm_hour, tm_d.tm_min, tm_d.tm_sec);
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
    if(res = curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L), res != CURLE_OK) goto on_error;
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
    curl_easy_cleanup(curl);
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
    if(hs) curl_slist_free_all(hs);
    if(fpw) fclose(fpw);
    if(ptrw) free(ptrw);
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
    if(fp)fclose(fp);
    if(ptr)free(ptr);
    close_curl_session(crl);
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
    const char* TIME_1st_PARAM = "time=";
    const char* TIME_FORMAT = "%Y%m%d%H%M%S";
    const char* TIME_REST_PARAMS = "&ntpen=1&tz=GMT&location=GMT&ck_dst=0&ntpserver=time.nist.gov";
    const char* MD_INIT_PARAMS = "recch=1&tapech=1&ts0=&ts1=&ts2=&ts3=&dealmode=536870912&rect0=0,0,999,999, 5&rect1=&rect2=&rect3=&chn=0";
    const char* SD_INIT_PARAMS = "tapech=1&recch=1&ts0=&ts1=&ts2=&ts3=&dealmode=536870912&enable=1&sensitivity=6";

    char* md_uri = NULL;
    char* sd_uri = NULL;
    char* time_uri = NULL;
    int ret = 0;

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

    pu_log(LL_INFO, "%s: Initiation parameters for MD %s", __FUNCTION__, MD_INIT_PARAMS);
    pu_log(LL_INFO, "%s: Initiation parameters for SD %s", __FUNCTION__, SD_INIT_PARAMS);
    pu_log(LL_INFO, "%s: Initiation parameters for TIME %s", __FUNCTION__, buf);
/* TODO if can't make directory - snaphots sohuld be disabled! */
    ac_make_directory(DEFAULT_DT_FILES_PATH, DEFAULT_SNAP_DIR);
    ac_make_directory(DEFAULT_DT_FILES_PATH, DEFAULT_VIDEO_DIR);

    ret = 1;
    on_error:
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
    int ret = 0;
    CURLcode res;
    FILE* fp=NULL;
    char* uri = NULL;

    CURL* crl = open_curl_session();
    if(!crl) return 0;

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
    if(fp)fclose(fp);
    if(uri) free(uri);
    close_curl_session(crl);
    return ret;
}
int ac_set_md(int on) {
    return update_one_parameter(AO_CAM_CMD_MD, AO_CAM_PAR_MD_ON, on);
}
int ac_set_sd(int on) {
    return update_one_parameter(AO_CAM_CMD_SD, AO_CAM_PAR_SD_ON, on);
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


