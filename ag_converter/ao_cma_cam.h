/*
 *  Copyright 2017 People Power Company
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
 Created by gsg on 30/10/17.
 Coding and decoding camera messages to/from internal presentation
*/

#ifndef IPCAMTENVIS_AO_CMA_CAM_H
#define IPCAMTENVIS_AO_CMA_CAM_H

#include <stddef.h>

#include "ao_cmd_data.h"


/*
 * Converts JSON to camera request
 * Returns resulting string or NULL if error
 * NB! returned memory should be freed after use!
 */
char* ao_cam_encode(const char* in);

/*
 * Converts camera request to JSON
 * Returns resulting string or NULL if error
 * NB! returned memory should be freed after use!
 */
char* ao_cam_decode(const char* in);

/*
 * Return camera event or 0 (AC_CAM_EVENT_UNDEF) if snth wrong
 */
t_ao_cam_alert ao_cam_decode_alert(const char* in);

/*
 * Notify Agent about the event
 * if start_date or end_date is 0 fields are ignored
 */
const char* ao_make_cam_alert(t_ac_cam_events event, time_t start_date, time_t end_date, char* buf, size_t size);

#define AO_CAM_CMD_SNAPSHOT 1
#define AO_CAM_CMD_MD       2
    #define AO_CAM_PAR_MD_SENS  1
    #define AO_CAM_PAR_MD_ONOFF 2
#define AO_CAM_CMD_SD       3
    #define AO_CAM_PAR_SD_SENS  1
    #define AO_CAM_PAR_SD_ONOFF 2

char* ao_make_cam_uri(int cmd_id);
char* ao_update_params_list(int cmd_id, int par_id, int par_value, char* lst);
char* ao_make_params_from_list(int cmd_id, char* lst);
int ao_get_param_from_list(int cmd_id, int par_id, char* lst);


#endif /* IPCAMTENVIS_AO_CMA_CAM_H */
