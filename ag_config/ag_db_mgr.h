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
 Created by gsg on 25/09/18.
 Module contains all interfaces and datatypes for properties dB
 This is common data & mapping for the cloud & the cam properties
*/

#ifndef IPCAMTENVIS_AG_DB_MGR_H
#define IPCAMTENVIS_AG_DB_MGR_H

#include "ao_cmd_data.h"

int ag_db_load_cam_properties();
void ag_db_unload_cam_properties();

typedef enum {
    AG_DB_ANY,      /* All types */
    AG_DB_OWN,      /* Agent's properties */
    AG_DB_CAM,      /* Camera's properties */
    AG_DB_CLOUD     /* Cloud properties */
} ag_db_property_type_t;
/*
 * filter - what types of property changes should be commited
 * return NULL terminated list of "{\"name\":\"<ParameterName>\", \"value\":\"<ParameterValue\", \"forward\":0}'\0'"
 * for all parameters which changed their values
 * NB1 Changes list deleted after use!
 * NB2 Returned list should be erased after use !!!
 */
char** ag_db_get_changes_report(ag_db_property_type_t filter);
void ag_erase_changes_report(ag_db_property_type_t filter)


/* Work with bool parameters - flags */
/* value set to 1 */
void ag_db_set_flag_on(const char* bool_property_name);
/* value set to 0 */
void ag_db_set_flag_off(const char* bool_property_name);
/*
 * return value
 */
int ag_db_get_flag(const char* bool_property_name);

/*
 * Return 0 if no change; return 1 if proprrty changed
 * property_value -> new_value
 */
int ag_db_store_property(const char* property_name, const char* property_value);

/*
 * Return value
 */
const char* ag_db_get_property(const char* property_name);
int ag_db_get_int_property(const char* property_name);

ag_db_property_type_t ag_get_property_type(const char* property_name);


/**************** Property names definitions */
/* AG_DB_OWN */
#define AG_DB_STATE_AGENT_ON    "state_agent_on"
#define AG_DB_CMD_CONNECT_AGENT     "connect_agent_cmd"
#define AG_DB_CMD_SEND_WD_AGENT     "send_wd_agent_cmd"

#define AG_DB_STATE_WS_ON       "state_ws_on"
#define AG_DB_CMD_CONNECT_WS        "connect_ws_cmd"
#define AG_DB_CMD_ASK_4_VIEWERS_WS  "ask_4_viewers_cmd"

#define AG_DB_STATE_RW_ON       "state_rw_on"
#define AG_DB_CMD_CONNECT_RW    "connect_rw_cmd"
#define AG_DB_CMD_DISCONNECT_RW "disconnect_rw_cmd"

/* AG_DB_CAM */
#define AG_DB_STATE_VIEWERS_COUNT   "wsViewersCount"
#define AG_DB_STATE_STREAM_STATUS   "streamStatus"

#endif /* IPCAMTENVIS_AG_DB_MGR_H */
