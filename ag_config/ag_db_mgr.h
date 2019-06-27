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
 Rule of use in outer space:
    1. Set/update values
    2. Make actions accordingly to dB values
    3. Set new values to the camera (if any) (cam_method is not NULL && changed == 1)
    4. Save new persistently kept data (if any) (persistent == 1 && changed == 1)
    3. Make reports to the cloud/WS (in_changes_report == 1 && change_flag == 1)
    4. Clear flags (change_flag=changed=0;)
*/

#ifndef IPCAMTENVIS_AG_DB_MGR_H
#define IPCAMTENVIS_AG_DB_MGR_H

#include "cJSON.h"

#include "ao_cmd_data.h"

/**
 * Initiate cam properties. Load defaults if no file was saved or from the file.
 * @return  - 0 if error, 1 if Ok
 */
int ag_db_load_cam_properties();

/**
 * Delete camera peoperties in memory
 */
void ag_db_unload_cam_properties();

/* Not used */
typedef enum {
    AG_DB_ANY,      /* All types */
    AG_DB_OWN,      /* Agent's properties */
    AG_DB_CAM,      /* Camera's properties */
    AG_DB_CLOUD     /* Cloud properties */
} ag_db_property_type_t;
/*
 * filter - what types of property changes should be commited
 * return cJSON array of [{"name":"<ParameterName>", "value":"<ParameterValue"}, ...]
 * for all parameters which changed their values
 * NB1 Changes list deleted after use!
 * NB2 Returned list should be erased after use !!!
 */
/**
 * Create the report to the cloud with Camera parameter changes.
 * for all parameters which changed their values
 * NB1 Changes list deleted after use!
 * NB2 Returned list should be erased after use !!!
 *
 * @return  - cJSON array of [{"name":"<ParameterName>", "value":"<ParameterValue"}, ...]
 */
cJSON* ag_db_get_changes_report();
/**
 * Create JSON report same format as above for all properties has to be reported at startup phase
 *
 * @return  - cJSON array of [{"name":"<ParameterName>", "value":"<ParameterValue"}, ...]
 */
cJSON* ag_db_get_startup_report();

/**
 * Set property by name.
 *
 * @param property_name
 * @param property_value - value in string format
 * @return  - 0 if no change, 1 if property changed
 */
int ag_db_set_property(const char* property_name, const char* property_value);

/**
 * Set peoperty by name
 *
 * @param property_name
 * @param property_value - int value
 * @return  - 0 if no change, 1 if property changed
 */
int ag_db_set_int_property(const char* property_name, int property_value);

/**
 * Get int property value by its name
 * @param property_name
 * @return  - property value
 */
int ag_db_get_int_property(const char* property_name);

/**
 * Cloud-Cam parameter set: set on cam, re-read and store into DB
 * If re-read value differs - dB change_flag On if same after update - Off
 * @return  - 1
 */
int ag_db_update_changed_cam_parameters();

/**
 * Save changes on disk only parameters marked as persistent will be saved
 */
void ag_db_save_persistent();

/**
 * Reset flags changed and change_flag at the end of cycle of use.
*/
void ag_clear_flags();

/*
 * Binary properties analysis
 */
typedef enum {
    AG_DB_BIN_UNDEF,
    AG_DB_BIN_NO_CHANGE,    /* no changes */
    AG_DB_BIN_OFF_OFF,      /* 0->0*/
    AG_DB_BIN_OFF_ON,       /* 0->1*/
    AG_DB_BIN_ON_OFF,       /* 1->0 */
    AG_DB_BIN_ON_ON         /* 1->1 */
} ag_db_bin_state_t;

/**
 * Get binary property status
 *
 * @param property_name
 * @return  - see the ag_db_bin_state_t above
 */
ag_db_bin_state_t ag_db_bin_anal(const char* property_name);



/**************** Property names definitions */
/* AG_DB_OWN */
#define AG_DB_STATE_AGENT_ON        "state_agent_on"
#define AG_DB_CMD_CONNECT_AGENT     "connect_agent_cmd"     /* obsolete */
#define AG_DB_CMD_SEND_WD_AGENT     "send_wd_agent_cmd"

#define AG_DB_STATE_WS_ON           "state_ws_on"
#define AG_DB_CMD_CONNECT_WS        "connect_ws_cmd"        /* obsolete */
#define AG_DB_CMD_ASK_4_VIEWERS_WS  "ask_4_viewers_cmd"
#define AG_DB_CMD_PONG_REQUEST      "send_pong_ws_cmd"

#define AG_DB_STATE_RW_ON           "state_rw_on"
#define AG_DB_CMD_CONNECT_RW        "connect_rw_cmd"        /* obsolete */
#define AG_DB_CMD_DISCONNECT_RW     "disconnect_rw_cmd"     /* obsolete */

/* AG_DB_CAM */
#define AG_DB_STATE_VIEWERS_COUNT   "viewersCount"              /* Active Viewers amount total */
#define AG_DB_STATE_PING_INTERVAL   "pingInterval"              /* WS ping interval  */
#define AG_DB_STATE_STREAM_STATUS   "ppc.streamStatus"          /* Command to initiate streaming */
#define AG_DB_STATE_RAPID_MOTION    "ppc.rapidMotionStatus"     /* ? */
#define AG_DB_STATE_MD              "motionStatus"              /* 1 if MD now */
#define AG_DB_STATE_SD              "audioStatus"               /* 1 if SD now */
#define AG_DB_STATE_RECORDING       "recordStatus"              /* Set by cam to 1 of recording */
#define AG_DB_STATE_RECORD_SECS     "ppc.recordSeconds"         /* ? */
#define AG_DB_STATE_MD_SENSITIVITY  "ppc.motionSensitivity"     /* Set the cam's sensitivity for MD */
#define AG_DB_STATE_MD_COUNTDOWN    "ppc.motionCountDownTime"   /* ? */
#define AG_DB_STATE_MD_ON           "ppc.motionActivity"        /* Set MD on/off */
#define AG_DB_STATE_SD_ON           "ppc.audioActivity"         /* Set SD on/off */
#define AG_DB_STATE_AUDIO           "audioStreaming"            /* allow audio streaming&recording */
#define AG_DB_STATE_VIDEO           "videoStreaming"            /* allow video streaming&recording */
#define AG_DB_STATE_VIDEOCALL       "supportsVideoCall"         /* ? */
#define AG_DB_STATE_SW_VERSION      "version"                   /* SW version */
#define AG_DB_STATE_MEM_AVAILABLE   "availableBytes"            /* ? */
#define AG_DB_STATE_RECORD_FULL     "ppc.recordFullDuration"    /* ? */
#define AG_DB_STATE_SNAPSHOT        "ppc.captureImage"          /* 1 - make snapshot */
#define AG_DB_STATE_SD_SENSITIVITY  "ppc.audioSensitivity"      /* set the cam's sensitivity for SD */
#define AG_DB_STATE_CAPTURE_VIDEO   "ppc.captureVideo"          /* Start/stop video recording */

#endif /* IPCAMTENVIS_AG_DB_MGR_H */
