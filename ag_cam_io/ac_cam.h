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
 * Make initial settings for the Camera
 */
int ac_cam_init();

/*
 * Send to cam in_msg, receive out_msg. Return 1 of OK, return 0 if not
 * NB! out_cmd.cam_exchange.msg is not decoded and has to be freed after use!
 */
int ac_cam_dialogue(const t_ao_cam_exchange in_msg, t_ao_cam_exchange* out_cmd);

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
 * Send the corresponding cam's property to the camera,
 * Read back the cam's property, update the db_properties
 * Return 1 if OK, 0 if no property found
 */
int ac_cam_update_property(const char* property_name);



#endif /* IPCAMTENVIS_AC_CAM_H */
