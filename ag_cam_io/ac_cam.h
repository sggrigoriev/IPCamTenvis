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

#ifndef IPCAMTENVIS_AC_CAM_H
#define IPCAMTENVIS_AC_CAM_H

#include <dirent.h>
#include "cJSON.h"
#include "ao_cmd_data.h"

/*
 * AC_CAM_STOP_MD -> DEFAULT_MD_FILE_POSTFIX
 * AC_CAM_STOP_SD -> DEFAULT_SD_FILE_POSTFIX
 * AC_CAM_MADE_SNAPSHOT -> DEFAULT_SNAP_FILE_POSTFIX
 * Anything else -> DEFAULT_UNDEF_FILE_POSTFIX
 */
const char* ac_get_event2file_type(t_ac_cam_events e);

typedef struct {
    const char* postfix;
    time_t start_date, end_date;
    DIR* root;
    struct dirent* root_ent;
    DIR* files_stor;
    struct dirent* files_stor_ent;
    char dir_name[PATH_MAX];
    char file_name[PATH_MAX];
    char* list;
    int no_entry;       /* 1 if we didn't read directories after open */
}ac_cam_fl_t;
/*
 * Create name as prefixYYYY-MM-DD_HHMMSSpostfix.ext, store it into buf
 * Return buf
 */
const char* ac_make_name_from_date(const char* prefix, time_t timestamp, const char* postfix, const char* ext, char* buf, size_t size);
/*
 * Makes path directory. If already exists - ok all other errors reported!
 */
void ac_make_directory(const char* path, const char* dir_name);
/************************************************************************/

/*
 * Make initial something for the Camera
 * Set correct string for SD & MD - everything is ready but ts0 just as "ts0="
 */
int ac_cam_init();
/*
 * Opposite...
 */
void ac_cam_deinit();
/*
 * Make pictire and store it by full_path
 * Return 0 if error
 * Return 1 if OK
 */
int ac_cam_make_snapshot(const char* full_path);
/*
 * Return 0 if error
 * Return 1 if OK
 */
int ac_cam_make_video();

/* on = 1 set On, set = 0 set Off */
int ac_set_md(int on);
int ac_set_sd(int on);
int ac_set_audio(int on);


/*
 * Set new value, re-read it from the cam and return it back
 */
int ac_set_sd_sensitivity(int value);
int ac_set_md_sensitivity(int value);



#endif /* IPCAMTENVIS_AC_CAM_H */
