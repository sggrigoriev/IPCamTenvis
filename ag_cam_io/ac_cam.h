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

#include "cJSON.h"
#include "ao_cmd_data.h"

/*
 * [name, ..., name] for all 3 types
 */
typedef struct {
    cJSON* md_arr;
    cJSON* sd_arr;
    cJSON* snap_arr;
} ac_cam_resend_queue_t;

ac_cam_resend_queue_t* ac_cam_create_not_sent();
int ac_cam_add_not_sent(ac_cam_resend_queue_t* q, char type, const char* name);
void ac_cam_delete_not_sent(ac_cam_resend_queue_t* q);

/*
 * AC_CAM_STOP_MD -> DEFAULT_MD_FILE_POSTFIX
 * AC_CAM_STOP_SD -> DEFAULT_SD_FILE_POSTFIX
 * AC_CAM_MADE_SNAPSHOT -> DEFAULT_SNAP_FILE_POSTFIX
 * Anything else -> DEFAULT_UNDEF_FILE_POSTFIX
 */
const char* ac_get_event2file_type(t_ac_cam_events e);
/*
 * Create the JSON array with full file names& path for alert
 * Return NULL if no files found
 * NB! Returned string should be freed!
 */
char* ac_cam_get_files_name(const char* type, time_t start_date, time_t end_date);
/*
 * Get all older than today files for all types
 * NB! md, sd, snap should be freed after use! NULL if no files diven type found
 */
void ac_get_all_files(char** md, char** sd, char** snap);
/*
 * Return empty string or all shit after the first '.' in file name
 */
const char* ac_cam_get_file_ext(const char* name);
/*
 * Return file size in bytes
 */
unsigned long ac_get_file_size(const char* name);
/*
 * Delete all files from directory
 */
void ac_cam_clean_dir(const char* path);
/*
 * Delete all directories which are empty and elder than today
 * Called from ac_cam_init()
 */
void ac_delete_old_dirs();
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
