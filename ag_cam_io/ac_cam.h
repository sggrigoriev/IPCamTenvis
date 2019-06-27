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
 Interface for camera's commands.
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
/**
 * Calculates file name postfix from event came. Obsolete because of cam's interface changing
 * @param e -   event (see  ao_cmd_data.h)
 * @return  - appropriate file nane postfix. (see ag_defaults DEFAULT_*_FILE_POSTFIX
 */
const char* ac_get_event2file_type(t_ac_cam_events e);

/* Obsolete because of Cam's interface change */
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

/**
 * Create name as prefixHHMMSSpostfix.ext
 *
 * @param prefix -      char string with file name prefix
 * @param timestamp -   time which be inserted in HHMMSS formet after the prefix
 * @param postfix -     char string with postfix
 * @param ext -         char string with extention
 * @param buf -         buffer to save the name constructed
 * @param size -        buffer size
 * @return  - pointer to the buffer with file name if Ok or empty string if error
 */
const char* ac_make_name_from_date(const char* prefix, time_t timestamp, const char* postfix, const char* ext, char* buf, size_t size);

/**
 * Make directory on path if it is not exist.
 *
 * @param path -        string with full path
 * @param dir_name -    string with directory name *
 */
void ac_make_directory(const char* path, const char* dir_name);

/**
 * Initiates cam's command interface
 * Set correct string for SD & MD - everything is ready but ts0 just as "ts0=" *
 */
int ac_cam_init();

/**
 * Close cam's command interface
 */
void ac_cam_deinit();

/**
 * Make picture and store it by full_path
 *
 * @param full_path -   file name with path to store the picture
 * @return  - 0 if error, 1 if Ok
 */
int ac_cam_make_snapshot(const char* full_path);

/**
 * Run video recording
 *
 * @return  - 0 if error, 1 if Ok
 */
int ac_cam_make_video();

/**
 * Motion Detection switcher
 * @param on -  1,2 - set on, -1,0 - set off
 * @return  - new parameter value
 */
int ac_set_md(int on);

/**
 * Sound Detection switcher NB! Made differently from MD. Possibly not supports the rule (-1, 2) parameters!
 * @param on -  1 - set on, 0 - set off
 * @return -    new parameter value
 */
int ac_set_sd(int on);

/**
 * Audio switcher
 * @param on    - 0 switch off. 1 switch on
 * @return  - new parameter value
 */
int ac_set_audio(int on);

/**
 * Change the Sound Detection sensitivity
 * Set new value, re-read it from the cam and return it back
 *
 * @param value -   sensitivity level to be set
 * @return  - new parameter value
 */
int ac_set_sd_sensitivity(int value);

/**
 * Change the Motion detection sensitivity
 * Set new value, re-read it from the cam and return it back*
 * @param value
 * @return  - new parameter value
 */
int ac_set_md_sensitivity(int value);

#endif /* IPCAMTENVIS_AC_CAM_H */
