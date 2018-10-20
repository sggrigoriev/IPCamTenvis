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


#include "ao_cmd_data.h"

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
 * Get file list with all existing files of given type
 * Return {"filesList":[]}
 * Return NULL if no files found
 * NB! Returned string should be freed!
 */
char* ac_get_all_files(const char* ft);
/*
 * Return empty string or all shit after the first '.' in file name
 */
const char* ac_cam_get_file_ext(const char* name);
/*
 * Return file size in bytes
 */
size_t ac_get_file_size(const char* name);
/*
 * Delete files from list.
 * file_list is a JSON array as ["name",...,"name"]
 */
void ac_cam_delete_files(const char* file_list);
/*
 * Delete all directories which are empty and elder than today
 * Called from ac_cam_init()
 */
void ac_delete_old_dirs();
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


/*
 * Set new value, re-read it from the cam and return it back
 */
int ac_set_sd_sensitivity(int value);
int ac_set_md_sensitivity(int value);


#endif /* IPCAMTENVIS_AC_CAM_H */
