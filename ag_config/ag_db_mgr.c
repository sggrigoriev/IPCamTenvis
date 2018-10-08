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
#include <limits.h>

#include "pu_logger.h"

#include "ag_defaults.h"
#include "ac_cam.h"

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
/* Function to convert values between cloud<->camera
 * Returned memory should be freed!
 * NB! input NULL checked!
 */
typedef char*(*i2c)(int value, char* buf, size_t size);
typedef int (*in_t)(const char* in_value);
typedef int (*out_t)(int in_value);

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))
#define ANAL(a)     if((!a)) {\
                        pu_log(LL_ERROR, "%s: Not enough memory", __FUNCTION__); \
                        goto on_error; \
                    }
#define FREE(a)     if((a)) { free(a); a = NULL;}

/*
 * Convertors. We got only 2 params, so make 4 functions and that's all
*/
/* cloud [0..100], cam [0..9] NB! cam's 0 corresponds to cloud's 1*/
static int SS_cloud_2_cam(int cloud_value) {
    if(cloud_value > 90) cloud_value = 90;
    return (cloud_value%10 >= 5)?cloud_value/10+1:cloud_value/10;
}
static int SS_cam_2_cloud(int cam_value) {
    return cam_value*10;
}
/* Cloud [0,10, 20, 30, 40]
 * Cam [0,1, 2, 3, 4, 5]
*/
static int MS_cloud_2_cam(int cloud_value) {
    return cloud_value/10;
}
static int MS_cam_2_cloud(int cam_value) {
    if(cam_value > 4) cam_value = 4;
    return cam_value*10;
}

typedef struct {
/*01*/  char* name;             /*Cloud or own name */
/*02*/  int value;            /* Could not be used if flag only. For OWN flags(commands) */
/*03*/  int change_flag;          /* If there is own logic for param - it will work if change_flag == 1 */

/*04*/  int in_startup_report;  /* 1: Should be taken for startup params report to cloud */
/*05*/  int changed;            /* 1 if value changed adn in_changes_report == 1. Set to 0 after ag_db_get_startup_report() call */
/*06*/  int in_changes_report;  /* 1: Sould be taken for changes report to WS if "change_flag == 1*/

/*07*/  int updated;            /* 1 if updated and persistent == 1 Set to 0 after ag_save_cam_properties() call */
/*08*/  int persistent;         /* 1 if has to be stored on disk */

/*09*/  int default_value;

/*10*/  out_t cloud_cam_converter;  /* Converts value to cam_value Could be NULL */
/*11*/  out_t cam_cloud_converter;  /* Converts cam_value to value Could be NULL */
/*12*/  out_t cam_method;           /* Function to take param from camera and return back after re-read(see ac_cam.h) */
} ag_db_record_t;
static const ag_db_record_t SCHEME[] = {
/*  1                       2   3   4   5   6   7   8   9   10                   11                  12  */
/*  name                    val chf str chd chr upd prs dfv clcam               camcl               camm */
{AG_DB_STATE_AGENT_ON,      0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_CMD_CONNECT_AGENT,   0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_CMD_SEND_WD_AGENT,   0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},

{AG_DB_STATE_WS_ON,         0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_CMD_CONNECT_WS,      0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_CMD_ASK_4_VIEWERS_WS,0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_CMD_PONG_REQUEST,    0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},

{AG_DB_STATE_RW_ON,         0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_CMD_CONNECT_RW,      0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_CMD_DISCONNECT_RW,   0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},

{AG_DB_STATE_VIEWERS_COUNT, 0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_PING_INTERVAL, 0,  0,  0,  0,  0,  0,  1,  30, NULL,               NULL,               NULL},
{AG_DB_STATE_STREAM_STATUS, 0,  0,  0,  0,  1,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_RAPID_MOTION,  0,  0,  1,  0,  1,  0,  1,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_MD,            0,  0,  1,  0,  1,  0,  0,  0,  NULL,               NULL,               ac_set_md},
{AG_DB_STATE_SD,            0,  0,  1,  0,  1,  0,  0,  0,  NULL,               NULL,               ac_set_sd},
{AG_DB_STATE_RECORDING,     0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_RECORD_SECS,   0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_MD_SENSITIVITY,0,  0,  1,  0,  1,  0,  1,  30, MS_cloud_2_cam,     MS_cam_2_cloud,     ac_set_md_sensitivity},
{AG_DB_STATE_MD_COUNTDOWN,  0,  0,  1,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_MD_ON,         0,  0,  0,  0,  1,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_SD_ON,         0,  0,  0,  0,  1,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_AUDIO,         0,  0,  0,  0,  0,  0,  0,  1,  NULL,               NULL,               NULL},
{AG_DB_STATE_VIDEO,         0,  0,  0,  0,  0,  0,  0,  1,  NULL,               NULL,               NULL},
{AG_DB_STATE_VIDEOCALL,     0,  0,  1,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_SW_VERSION,    0,  0,  1,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL}, /*TODO: Take out of here! */
{AG_DB_STATE_MEM_AVAILABLE, 0,  0,  0,  0,  0,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_RECORD_FULL,   0,  0,  1,  0,  0,  0,  0,  1,  NULL,               NULL,               NULL},
{AG_DB_STATE_SNAPSHOT,      0,  0,  1,  0,  1,  0,  0,  0,  NULL,               NULL,               NULL},
{AG_DB_STATE_SD_SENSITIVITY,0,  0,  1,  0,  1,  0,  1,  30, SS_cloud_2_cam,     SS_cam_2_cloud,     ac_set_sd_sensitivity}
/*  name                    val chf str chd chr upd prs dfv clcam               camcl               camm */
/*  1                       2   3   4   5   6   7   8   9   10                  11                  12   */
};

static ag_db_record_t *IMDB = 0;
static int char2int(const char* str) {
    int ret;
    if(!str) return 0;
    sscanf(str, "%d", &ret);
    return ret;
}
/*
 * return 1 if v1 == v2
 */
static int equal(const char* in, int own) {
    return char2int(in) == own;
}

/*
 * Return string {"params_array":[{"name":"<name>", "value":"<value>"}, ...]} or NULL
 */
static char* get_persistent_data(const char* file_name) {
    FILE* fd = fopen(file_name, "r");
    if(!fd) {
        pu_log(LL_WARNING, "%s: %s not found. Default values will be used", __FUNCTION__, file_name);
        return NULL;
    }
    fseek(fd, 0L, SEEK_END);
    long sz = ftell(fd);
    fseek(fd, 0L, SEEK_SET);
    if(sz <= 0) {
        pu_log(LL_WARNING, "%s: %s Got zero size.", __FUNCTION__, file_name);
        fclose(fd);
        return NULL;
    }
    char* JSON_string = calloc((size_t)sz, 1);
    if(!JSON_string) {
        pu_log(LL_ERROR, "%s: Not enough memory", __FUNCTION__);
        fclose(fd);
        return NULL;
    }
    return JSON_string;
}

/*
 * Return IMDB index if found or -1 if not
 * search by name filed
 * NB! add not found into log!!
 */
static int find_param(const char* name) {
    int i;
    if(!name) return -1;
    for(i = 0; i < NELEMS(SCHEME); i++) {
        if(!strcmp(IMDB[i].name, name)) return i;
    }
    pu_log(LL_WARNING, "%s Parameter %s not found in dB", __FUNCTION__, name);
    return -1;
}
/*
 * Set change_flag & updated flags unconditionally!
 */
static void replace_int_param_value(int idx, int value) {
    IMDB[idx].value = value;
    if(IMDB[idx].in_changes_report) IMDB[idx].changed = 1;
    if(IMDB[idx].persistent) IMDB[idx].updated = 1;
}
static void replace_param_value(int idx, const char* value) {
    if(!value) return;
    int val = char2int(value);
    replace_int_param_value(idx, val);
}

static int create_imdb() {
    IMDB = NULL;
    IMDB = calloc(sizeof(ag_db_record_t), NELEMS(SCHEME));
    int i;
    for(i = 0; i < NELEMS(SCHEME); i++) {
/*01*/  IMDB[i].name = strdup(SCHEME[i].name); ANAL(IMDB[i].name);
/*02*/  IMDB[i].value = SCHEME[i].default_value;
/*03*/  /*change_flag = 0 */
/*04*/  IMDB[i].in_startup_report = SCHEME[i].in_startup_report;
/*05*/  /* changed = 0 */
/*06*/  IMDB[i].in_changes_report = SCHEME[i].in_changes_report;
/*07*/  /* updated = 0 */
/*08*/  IMDB[i].persistent = SCHEME[i].persistent;
/*09*/  /* default_value not needed and not used */
/*10*/  IMDB[i].cloud_cam_converter = SCHEME[i].cloud_cam_converter;
/*11*/  IMDB[i].cam_cloud_converter = SCHEME[i].cam_cloud_converter;
/*12*/  IMDB[i].cam_method = SCHEME[i].cam_method;
    }
    return 1;
on_error:
    ag_db_unload_cam_properties();
    return 0;
}
/*
 * Persistent data format: {"params_array":[{"name":"<name>", "value":"<value>"}, ...]}
 */
static int load_persistent_data() {
    int ret = 0;

    char* JSON_string = get_persistent_data(DEFAULT_DB_PATH);
    if(!JSON_string) return ret;

    cJSON* data = cJSON_Parse(JSON_string);

    if(!data) {
        pu_log(LL_ERROR, "%s: Error JSON parsing %s.", __FUNCTION__, JSON_string);
        goto on_error;
    }
    cJSON* array = cJSON_GetObjectItem(data, "params_array");
    if(!array || !cJSON_GetArraySize(array)) {
        pu_log(LL_ERROR, "%s: Empty parameters aray %s.", __FUNCTION__, JSON_string);
        goto on_error;
    }
    int i;
    for (i = 0; i < cJSON_GetArraySize(array); i++) {
        cJSON* item = cJSON_GetArrayItem(array, i);
        if(!item) {
            pu_log(LL_WARNING, "%s: Empty item #%d found in %s. Ignored", __FUNCTION__, i, JSON_string);
            continue;
        }
        cJSON* name = cJSON_GetObjectItem(item, "name");
        cJSON* value = cJSON_GetObjectItem(item, "value");
        if(!name || !value) {
            pu_log(LL_WARNING, "%s: name or value not found in %dth item in %s. Item ignored", __FUNCTION__, i, JSON_string);
            continue;
        }
        int pos = find_param(name->valuestring);
        if(pos < 0) {
            pu_log(LL_WARNING, "%s: %dth parameter %s is not found in IMDB. Ignored", __FUNCTION__, i, item->valuestring);
            continue;
        }
        replace_param_value(pos, value->valuestring);
    }
    ret = 1;    /* we're here if OK */
on_error:
    FREE(JSON_string);
    FREE(data);
    return ret;
}
/*
 * Set cam_parameters to IMDB values:
 *      if parameter <> cam parameter -> call cam, read param again
 *      if new parameter <> cam parameter -> store new one to IMDB
 *      Return new cloud value
 */
static int sync_camera_param(int idx) {
    if(!IMDB[idx].cam_method) return 0; /* Not a cam parameter */

    int cam_value = (IMDB[idx].cloud_cam_converter)?IMDB[idx].cloud_cam_converter(IMDB[idx].value):IMDB[idx].value;

    cam_value = IMDB[idx].cam_method(cam_value);

    int new_cloud_value =(IMDB[idx].cam_cloud_converter)?IMDB[idx].cam_cloud_converter(cam_value):cam_value;
    if(new_cloud_value != IMDB[idx].value) {
        replace_int_param_value(idx, new_cloud_value);
    }
    return new_cloud_value;
}
/*
 * 1. Initialize camera data: upload cam parameters from camera
 * 2. Set all cam_parameter to IMDB values
 */
static int sync_camera_data() {
    int i;
    for(i = 0; i < NELEMS(SCHEME); i++) {
        sync_camera_param(i);
    }
    return 1;
}
/*
 * 1. Create IMDB based on SCHEME, update all values by defaults
 * 2. Load persistent data (if any), update values & cam values
 * 3. Synchronize camera data
 * 4. Return 1 if OK, 0 if not
 */
int ag_db_load_cam_properties() {
    if(!create_imdb()) {
        pu_log(LL_ERROR, "%s: Error IMdB creation", __FUNCTION__);
        return 0;
    }
    pu_log(LL_ERROR, "%s: IMdB created OK", __FUNCTION__);
    if(load_persistent_data()) pu_log(LL_DEBUG, "%s: Persistent data loaded OK", __FUNCTION__);

    if(!sync_camera_data()) {
        pu_log(LL_ERROR, "%s: Error load camera data", __FUNCTION__);
        ag_db_unload_cam_properties();
        return 0;
    }
    pu_log(LL_DEBUG, "%s: Camera data data loaded OK", __FUNCTION__);
    return 1;
}
/*
 * Free IMDB,
 * Free CamDB
 */
void ag_db_unload_cam_properties() {
    if(!IMDB) return;
    int i;
    for(i = 0; i < NELEMS(SCHEME); i++) {
        FREE(IMDB[i].name);
    }
    FREE(IMDB);
};
/* Save persistent data to disk */
int ag_save_cam_properties() {
//TODO!!! updated flag set t 0!
    return 0;
}

static void add_reported_property(cJSON* report, const char* name, int value) {
    cJSON* obj = cJSON_CreateObject();
    cJSON* par_name = cJSON_CreateString(name);
    char buf[10]={0};
    snprintf(buf, sizeof(buf)-1, "%d", value);
    cJSON* par_value = cJSON_CreateString(buf);

    cJSON_AddItemToObject(obj, "name", par_name);
    cJSON_AddItemToObject(obj, "value", par_value);
    cJSON_AddItemToArray(report, obj);
}
/*
 * return cJSON array of [{"name":"<ParameterName>", "value":"<ParameterValue"}, ...]
*/
cJSON* ag_db_get_changes_report() {
    cJSON* rep = cJSON_CreateArray(); ANAL(rep);
    int i;
    for(i = 0; i < NELEMS(SCHEME); i++) {
        if(IMDB[i].in_changes_report && IMDB[i].changed) {
            add_reported_property(rep, IMDB[i].name, IMDB[i].value);
            IMDB[i].changed = 0;
        }
    }
on_error:
    FREE(rep);
    return NULL;
}
cJSON* ag_db_get_startup_report() {
    cJSON* rep = cJSON_CreateArray(); ANAL(rep);
    int i;
    for(i = 0; i < NELEMS(SCHEME); i++) {
        if(IMDB[i].in_startup_report) add_reported_property(rep, IMDB[i].name, IMDB[i].value);
    }
on_error:
    if(rep) cJSON_Delete(rep);
    return NULL;
}
void ag_erase_changes_report() {
    int i;
    for(i = 0; i < NELEMS(SCHEME); i++) {
        if(IMDB[i].in_changes_report && IMDB[i].change_flag) IMDB[i].change_flag = 0;
    }
}


static void set_flag(const char* property_name, int value) {
    int pos = find_param(property_name);
    if(pos < 0) return;
    IMDB[pos].change_flag = value;
}
/* Work with property's flags */
/* property's flag value set to 1 */
void ag_db_set_flag_on(const char* property_name) {
    set_flag(property_name, 1);
}
/* property's flag value set to 0 */
void ag_db_set_flag_off(const char* property_name) {
    set_flag(property_name, 0);
}
/*
 * return property's flag value
 */
int ag_db_get_flag(const char* property_name) {
    int pos = find_param(property_name);
    if(pos < 0) return 0;
    return IMDB[pos].change_flag;
}
/*
 * Return 0 if no change; return 1 if proprrty changed
 * !Set the property's flag ON anyway.
 */
int ag_db_store_property(const char* property_name, const char* property_value) {
    int ret=0;
    int pos = find_param(property_name);
    if(pos < 0) return 0;
    if(!equal(property_value, IMDB[pos].value)) {
        replace_param_value(pos, property_value);
        ret = 1;
    }
    IMDB[pos].change_flag = 1;
    return ret;
}
int ag_db_store_int_property(const char* property_name, int property_value) {
    char buf[20]={0};
    snprintf(buf, sizeof(buf)-1, "%d", property_value);
    return ag_db_store_property(property_name, buf);
}
int ag_db_get_int_property(const char* property_name) {
    int pos = find_param(property_name);
    if(pos < 0) return 0;

    return IMDB[pos].value;
}
/*
 * Cloud-Cam parameters set: set on cam, re-read and store into DB
 * If param was changed by cloud update - set it on Cam.
 * If returned from cam value <> cloud update - set change flag On
 */
int ag_db_update_changed_cam_parameters() {
    int i;
    for(i = 0; i < NELEMS(SCHEME); i++) {
        if(IMDB[i].change_flag) {
            IMDB[i].change_flag = 0;
            sync_camera_param(i);
        }
    }
    return 1;
}



