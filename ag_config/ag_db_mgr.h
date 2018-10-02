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
 All functions except load/unload are thread-protected!
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
void ag_erase_changes_report(ag_db_property_type_t filter);
/*
 * Create JSON report same format as above for all properties has to be reported at startup phase
 */
char** ag_db_get_startup_report();


/* Work with property's flags */
/* property's flag value set to 1 */
void ag_db_set_flag_on(const char* property_name);
/* property's flag value set to 0 */
void ag_db_set_flag_off(const char* property_name);
/*
 * return property's flag value
 */
int ag_db_get_flag(const char* property_name);

/*
 * Return 0 if no change; return 1 if proprrty changed
 * !Set the property's flag ON in any case!.
 */
int ag_db_store_property(const char* property_name, const char* property_value);

int ag_db_get_int_property(const char* property_name);


/**************** Property names definitions */
/* AG_DB_OWN */
#define AG_DB_STATE_AGENT_ON        "state_agent_on"
#define AG_DB_CMD_CONNECT_AGENT     "connect_agent_cmd"
#define AG_DB_CMD_SEND_WD_AGENT     "send_wd_agent_cmd"

#define AG_DB_STATE_WS_ON           "state_ws_on"
#define AG_DB_CMD_CONNECT_WS        "connect_ws_cmd"
#define AG_DB_CMD_ASK_4_VIEWERS_WS  "ask_4_viewers_cmd"
#define AG_DB_CMD_PONG_REQUEST      "send_pong_ws_cmd"

#define AG_DB_STATE_RW_ON           "state_rw_on"
#define AG_DB_CMD_CONNECT_RW        "connect_rw_cmd"
#define AG_DB_CMD_DISCONNECT_RW     "disconnect_rw_cmd"

/* AG_DB_CAM */
#define AG_DB_STATE_VIEWERS_COUNT   "viewersCount"
#define AG_DB_STATE_PING_INTERVAL   "pingInterval"
#define AG_DB_STATE_STREAM_STATUS   "ppc.streamStatus"
#define AG_DB_STATE_RAPID_MOTION    "ppc.rapidMotionStatus"
#define AG_DB_STATE_MD              "motionStatus"
#define AG_DB_STATE_SD              "audioStatus"
#define AG_DB_STATE_RECORDING       "recordStatus"
#define AG_DB_STATE_RECORD_SECS     "ppc.recordSeconds"
#define AG_DB_STATE_MD_SENSITIVITY  "ppc.motionSensitivity"
#define AG_DB_STATE_MD_COUNTDOWN    "ppc.motionCountDownTime"
#define AG_DB_STATE_MD_ON           "ppc.motionActivity"
#define AG_DB_STATE_SD_ON           "ppc.audioActivity"
#define AG_DB_STATE_AUDIO           "audioStreaming"
#define AG_DB_STATE_VIDEO           "videoStreaming"
#define AG_DB_STATE_VIDEOCALL       "supportsVideoCall"
#define AG_DB_STATE_SW_VERSION      "version"
#define AG_DB_STATE_MEM_AVAILABLE   "availableBytes"
#define AG_DB_STATE_RECORD_FULL     "ppc.recordFullDuration"
#define AG_DB_STATE_SNAPSHOT        "ppc.captureImage"
#define AG_DB_STATE_STREAMERROR     "streamError"
#define AG_DB_STATE_SD_SENSITIVITY  "ppc.audioSensitivity"

#endif /* IPCAMTENVIS_AG_DB_MGR_H */
