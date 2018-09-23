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

#include "pu_logger.h"

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

int ac_cam_init() {
}

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
        pu_log(LL_ERROR, "%s: Wrong event %s only %s or %s expected", __FUNCTION__, ac_cam_evens2string(data.cam_event), ac_cam_evens2string(AC_CAM_STOP_MD), ac_cam_evens2string(AC_CAM_STOP_SD));
        buf[0] = '\0';
        return buf;
    }
    strncpy(buf, "filesList: ", size-1);
    int len = strlen(buf);
    add_files_list(dir, data.start_date, data.end_date, postfix, buf, size);
    if(strlen(buf) <= len) {
        pu_log(LL_WARNING, "%s: no files were found", __FUNCTION__);
        buf[0] = '\0';
    }
    return buf;
}