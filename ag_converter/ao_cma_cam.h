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
 See the https://presence.atlassian.net/wiki/spaces/EM/pages/76873772/C1+IPCamera?preview=/76873772/574914695/CTP(Command%20Transport%20Protocol)%20%26%20CGI.2.docx
*/

#ifndef IPCAMTENVIS_AO_CMA_CAM_H
#define IPCAMTENVIS_AO_CMA_CAM_H

#include <stddef.h>
#include "cJSON.h"
#include "ao_cmd_data.h"

/*
 * Return camera event or 0 (AC_CAM_EVENT_UNDEF) if snth wrong
 */
/**
 * Translate the cam alert as {"alertName" : "<name>[, "startDate" : <time_t>[, "endDate" : time_t]]}
 * to structure. See the ao_cmd_data.h
 *
 * @param obj   - parsed JSON message
 * @return  - camera event structure with event type. ET = AC_CAM_EVENT_UNDEF if error
 */
t_ao_cam_alert ao_cam_decode_alert(cJSON* obj);


#define AO_CAM_READ     1
#define AO_CAM_WRITE    0

/* Cam commands id */
#define AO_CAM_CMD_SNAPSHOT 1
#define AO_CAM_CMD_MD       2
#define AO_CAM_CMD_SD       3
#define AO_CAM_CMD_TIME     4
#define AO_CAM_CMD_CFGREC   5
#define AO_CAM_CMD_CAPTURE_VIDEO    6
#define AO_CAM_CMD_H264     7
#define AO_CAM_CMD_SETVIDEO 8
#define AO_CAM_CMD_OSD      9

/* External parameters codes */
typedef enum {
    AO_CAM_PAR_UNDEF,
    AO_CAM_PAR_MD_SENS, AO_CAM_PAR_MD_ON,
    AO_CAM_PAR_SD_SENS, AO_CAM_PAR_SD_ON,
    AO_CAM_PAR_CFGREC_AUDIO_ON,
    AO_CAM_PAR_SIZE
} user_par_t;

/* Camera parameters codes */
typedef enum {EP_UNDEFINED,
    EP_RECCH, EP_TAPECH,
    EP_TS0, EP_TS1, EP_TS2, EP_TS3,
    EP_DEALMODE, EP_ENABLE, EP_SESETIVITY,
    EP_RECT0, EP_RECT1, EP_RECT2, EP_RECT3,
    EP_CHN,
/* CFGREC */
            EP_SIZELMT, EP_TIMELMT, EP_VSTRM, EP_ALRMTRGREC, EP_SNAP_INSTEAD, EP_RECORD_AUDIO, EP_SNAP_INTERVAL,
    EP_SIZE
} par_t;

/**
 * Make HTTP request to the Cam for the command for read or write
 * NB-1! returned memory should be freed after use
 *
 * @param cmd_id    - command ID (AO_CAM_CMD_*)
 * @param read_pars - 1 if the command got parameters, 0 if command w/o parameters
 * @return  - pointer constructed URI or NULL if error.
 */
char* ao_make_cam_uri(int cmd_id, int read_pars);

/**
 * Save parameter locally
 *
 * @param cmd_id    - command id (see AO_CAM_CMD_*)
 * @param par_id    - parameter ID
 * @param par_value - parameter value
 */
void ao_save_parameter(int cmd_id, user_par_t par_id, int par_value);

/**
 * Extract params from lst and save it in local store.
 * lst format: name value\r\n...name value\r\n\r\n
 * Used to save params came from camera
 *
 * @param cmd_id    - command id  (see AO_CAM_CMD_*)
 * @param lst       - parameters list
 */
void ao_save_params(int cmd_id, const char* lst);

/**
 * Create params list for the command from local store and return as lst for Camera
 * NB! returned lst sould be freed after use!
 *
 * @param cmd_id    - command id  (see AO_CAM_CMD_*)
 * @return  - parameters list
 */
char* ao_make_params(int cmd_id);
/*
 * Get cmd's parameter
 */
/**
 * Get command's parameter value by parameter name
 *
 * @param cmd_id    - command id (see AO_CAM_CMD_*)
 * @param par_id    - parameter id
 * @return  - parameter value or 0 if error (0 also could be the value, so this is not correct error rc!)
 */
int ao_get_param_value(int cmd_id, user_par_t par_id);

#endif /* IPCAMTENVIS_AO_CMA_CAM_H */