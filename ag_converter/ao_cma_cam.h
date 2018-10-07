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
 * Return camera event or 0 (AC_CAM_EVENT_UNDEF) if snth wrong
 */
t_ao_cam_alert ao_cam_decode_alert(const char* in);
/*
 * Notify Agent about the event
 * if start_date or end_date is 0 fields are ignored
 */
const char* ao_make_cam_alert(t_ac_cam_events event, time_t start_date, time_t end_date, char* buf, size_t size);

#define AO_CAM_READ     1
#define AO_CAM_WRITE    0

#define AO_CAM_CMD_SNAPSHOT 1
#define AO_CAM_CMD_MD       2
#define AO_CAM_CMD_SD       3

typedef enum {
    AO_CAM_PAR_UNDEF,
    AO_CAM_PAR_MD_SENS, AO_CAM_PAR_MD_ON,
    AO_CAM_PAR_SD_SENS, AO_CAM_PAR_SD_ON,
    AO_CAM_PAR_SIZE
} user_par_t;

typedef enum {EP_UNDEFINED,
    EP_RECCH, EP_TAPECH,
    EP_TS0, EP_TS1, EP_TS2, EP_TS3,
    EP_DEALMODE, EP_ENABLE, EP_SESETIVITY,
    EP_RECT0, EP_RECT1, EP_RECT2, EP_RECT3,
    EP_CHN,
    EP_SIZE
} par_t;

/*
 * NB-1! returned memory should be freed after use
 * NB-2! char* lis parameter frees inside!
 */
char* ao_make_cam_uri(int cmd_id, int read_pars);
/*
 * Save to local store param from dB
 */
void ao_save_parameter(int cmd_id, user_par_t par_id, int par_value);
/*
 * extract params from lst and save it in local store
 */
void ao_save_params(int cmd_id, const char* lst);
/*
 * create params list from local store and return ub lst
 * NB! lst sould be freed after use!
 */
char* ao_make_params(int cmd_id);
/*
 * Get cmd's parameter
 */
int ao_get_param_value(int cmd_id, user_par_t par_id);

#endif /* IPCAMTENVIS_AO_CMA_CAM_H */
