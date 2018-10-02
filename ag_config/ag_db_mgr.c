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
 Cloud & cam properties db
*/
#include <string.h>

#include "pu_logger.h"

#include "ag_defaults.h"
#include "ag_db_mgr.h"


/*
Own properties list
 state_agent_on: 0/1 -- offline or connected
 connect_agent_cmd: 0/1  -- 0 - no_command, 1 - (re)connect!
 send_wd_agent_cmd: 0/1 -- 0 - no command, 1 - send request

 state_ws_on: 0/1    -- offline or connected
 connect_ws_cmd: 0/1    -- 0 - no_command, 1 - (re)connect!
 send_pong_ws_cmd: 0/1  -- 0 - no command, 1 - send pong!
 ask_4_viewers_cmd: 0/1 -- 0 - no_command, 1 - send request

 state_rw_on: 0/1    -- offline or online
 connect_rw_cmd: 0/1        -- 0 - no command, 1 - (re)connect
 disconnect_rw_cmd: 0/1     -- 0 - no command, 1 - disconnect

Cam properties list
 viewersCount: int set when WS sends the "viewersCount"  Actions: from >0 to 0 -> stop streaming other changes -> no action
 pingInterval: int set whin WS send the "pingInterval"
 ppc.streamStatus: 0/1  0 - no streaming or stop streaming. 1 - streaming required
 ppc.rapidMotionStatus: The time between each new recording in seconds. 60 - 3600
 motionStatus: Measurement and status from the camera declaring if this camera is currently recording video. -1..2
 audioStatus: Whether audio detection is turned on or off. -1..2
 recordStatus: Measurement and command from the camera declaring if this camera is currently recording video or audio.
 ppc.recordSeconds: The set maximum length of each recording 5..300 on the cam is 20..600	Can't set it on camera!
 ppc.motionSensitivity: The motion sensitivity of the camera 0..40/cam limits 0..5
 ppc.motionCountDownTime: The initial countdown time when motion recording is turned on. 5..60. Use for MD and SD - both Can't set on Cam!
 ppc.motionActivity: Current state of motion activity.0/1	-- should be sent when alert starts (1) and when it stops (0)
 ppc.audioActivity: Current state of audio activity.0/1	-- should be sent when alert starts (1) and when it stops (0)
 audioStreaming: Sets audio streaming and recording on this camera.0/1 -- Just internal thing - how to configure the stream
 videoStreaming: Sets video streaming on this camera. 0/1  - Just internal thing how to configure the stream
 supportsVideoCall: 0 and not changed!
 version: The current version of Presence running on this device. String using the format “x.x.x”
 availableBytes: The amount of memory (in bytes) available on this device
 ppc.recordFullDuration: 1 and can't be changed!
 ppc.captureImage: Measurement/command
 streamError: Value is string. Should be sent to WS in case of streaming error
 ppc.audioSensitivity: The audio sensitivity of the camera

*/
/* Function to convert values between cloud<->camera */
typedef const char* (*converter_t)(const char* in_value);
typedef enum {rundef=0, rmin_max=1, rlist=2} rule_t;

typedef struct {
/*01*/    char* name;             /*Cloud or own name */
/*02*/    char* value;            /* Could be NULL if flag only. For OWN flags(commands) */
/*03*/    int change_flag;
/*04*/    int in_startup_report;  /* 1: Should be taken for startup params report to cloud */
/*05*/    int in_changes_report;  /* 1: Sould be taken for changes report to WS if "changed == 1*/
/*06*/    int updated;            /* 1 if updated and not reported. Set to 0 after ag_erase_changes_report() call */

/*07*/    char* cam_name;         /*Corresponding name in Camera. NULL if no correspondence */
/*08*/    char* cam_value;

/*09*/    char* default_value;

/*10*/    converter_t cloud_cam_converter;    /* Converts value to cam_value Could be NULL */
/*11*/    converter_t cam_cloud_converter;    /* Converts cam_value to value Could be NULL */

/*12*/    rule_t rule;                        /* Rule for convertors */
/*13*/    char* limits;                       /*Min, Max for rmin_max rule and v1,...,vN for rlist rule */
} ag_db_record_t;

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))
#define ANAL(a)     if((!a)) {\
                        pu_log(LL_ERROR, "%s: Not enough memory", __FUNCTION__); \
                        goto on_error; \
                    }
static const ag_db_record_t SCHEME[] = {
/*  1       2       3   4   5   6   7       8       9       10      11      12          13*/
{"mama",    NULL,   0,  0,  0,  0,  NULL,   NULL,   "4",    NULL,   NULL,   rmin_max,   "1\n2\n"}
};

static ag_db_record_t *IMDB = 0;

static int create_imdb() {
    IMDB = NULL;
    IMDB = calloc(sizeof(ag_db_record_t), NELEMS(SCHEME));
    int i;
    for(i = 0; i < NELEMS(SCHEME); i++) {
/*01*/  IMDB[i].name = strdup(SCHEME[i].name); ANAL(IMDB[i].name);
/*02*/  IMDB[i].value = strdup(SCHEME[i].default_value); ANAL(IMDB[i].value);
/*03*/  /*change_flag = 0 */
/*04*/  IMDB[i].in_startup_report = SCHEME[i].in_startup_report;
/*05*/  IMDB[i].in_changes_report = SCHEME[i].in_changes_report;
/*06*/  /* updated = 0 */
/*07*/  if(SCHEME[i].cam_name) {IMDB[i].cam_name = strdup(SCHEME[i].cam_name); ANAL(IMDB[i].cam_name);}
/*07*/  if(SCHEME[i].cam_value) {IMDB[i].cam_value = strdup(SCHEME[i].cam_value); ANAL(IMDB[i].cam_value);}
/*09*/  /* default_value = NULL not needed and not used */
/*10*/  IMDB[i].cloud_cam_converter = SCHEME[i].cloud_cam_converter;
/*11*/  IMDB[i].cam_cloud_converter = SCHEME[i].cam_cloud_converter;
/*12*/  IMDB[i].rule = SCHEME[i].rule;
/*13*/  if(SCHEME[i].limits) {IMDB[i].limits = strdup(SCHEME[i].limits); ANAL(IMDB[i].limits);}
    }
    return 1;
on_error:
    ag_db_unload_cam_properties();
    return 0;
}
static int load_persistent_data() {
    FILE* data = fopen(DEFAULT_DB_PATH, "r");
    if(!data) {
        pu_log(LL_WARNING, "%s: %s not found. Default values will be used", __FUNCTION__, DEFAULT_DB_PATH);
        return 1;
    }

    return 0;
}
static int load_camera_data() {
    return 0;
}
/*
 * 1. Create IMDB based on SCHEME, update all values by defaults
 * 2. Load persistent data (if any), update values & cam values
 * 3. Load camera data
 * 4. Return 1 if OK, 0 if not
 */
int ag_db_load_cam_properties() {
    if(!create_imdb()) {
        pu_log(LL_ERROR, "%s: Error IMdB creation", __FUNCTION__);
        return 0;
    }
    pu_log(LL_ERROR, "%s: IMdB created OK", __FUNCTION__);
    if(!load_persistent_data()) {
        pu_log(LL_ERROR, "%s: Error load persistent data", __FUNCTION__);
        ag_db_unload_cam_properties();
        return 0;
    }
    pu_log(LL_DEBUG, "%s: Persistent data loaded OK", __FUNCTION__);
    if(!load_camera_data()) {
        pu_log(LL_ERROR, "%s: Error load camera data", __FUNCTION__);
        ag_db_unload_cam_properties();
        return 0;
    }
    pu_log(LL_DEBUG, "%s: Camera data data loaded OK", __FUNCTION__);
    return 1;
}
void ag_db_unload_cam_properties() {

};
