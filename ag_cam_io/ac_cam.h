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
 * Make initial settings for the Camera
 */
int ac_cam_init();
/*
 * Free camDB
 */
void ac_cam_deinit();


/*
 * Create the JSON array with full file names& path for alert
 */
const char* ac_cam_get_files_name(t_ao_cam_alert data, char* buf, size_t size);

/*
 * Make pictire and store it by full_path
 * Return 0 if error
 * Return 1 if OK
 */
int ac_cam_make_snapshot(const char* full_path);

/*
 * set cam_value to the cam:
 *  if cam's db has same value - no call camera
 *  else - send to cam, re-read, return what came from cam; store it into cam's db as well
 */
int ac_update_cam_parameter(const char* par_name, int par_value);

int ac_set_md_on();
int ac_set_md_off();
int ac_set_sd_on();
int ac_set_sd_off();


#endif /* IPCAMTENVIS_AC_CAM_H */
