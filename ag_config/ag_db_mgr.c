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
typedef int (*in_t)(const char* in_value);
typedef int (*out_t)(int in_value);
typedef enum {rundef=0, rmin_max=1, rlist=2} rule_t;

typedef struct {
/*01*/  char* name;             /*Cloud or own name */
/*02*/  int value;            /* Could not be used if flag only. For OWN flags(commands) */

/*03*/  int change_flag;          /* If there is own logic for param - it will work if change_flag == 1 */
/*04*/  int in_startup_report;  /* 1: Should be taken for startup params report to cloud */
/*05*/  int in_changes_report;  /* 1: Sould be taken for changes report to WS if "changed == 1*/
/*06*/  int updated;            /* 1 if updated and not reported. Set to 0 after ag_erase_changes_report() call */
/*07*/  int persistent;         /* 1 if has to be stored on disk */

/*08*/  char* cam_name;         /*Corresponding name in Camera. NULL if no correspondence */

/*09*/  int default_value;

/*10*/  in_t input_converter;        /*Adjusts input value to the cloud accordingly rules & limits */
/*11*/  out_t cloud_cam_converter;    /* Converts value to cam_value Could be NULL */
/*12*/  out_t cam_cloud_converter;    /* Converts cam_value to value Could be NULL */

/*13*/    rule_t rule;                        /* Rule for convertors */
/*14*/    int limits[];                       /*Min, Max for rmin_max rule and v1,...,vN, INT_MAX for rlist rule */
} ag_db_record_t;

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))
#define ANAL(a)     if((!a)) {\
                        pu_log(LL_ERROR, "%s: Not enough memory", __FUNCTION__); \
                        goto on_error; \
                    }
#define FREE(a)     if((a)) { free(a); /*a = NULL;*/}

static const ag_db_record_t SCHEME[] = {
/*  1       2   3   4   5   6   7   8       9   10      11      12      13          14      */
{"mama",    0,  0,  0,  0,  0,  1,  NULL,   4,  NULL,   NULL,   NULL,   rmin_max,   {1,2, INT_MAX}}
};

static ag_db_record_t *IMDB = 0;
int char2int(const char* str) {
    int ret;
    if(!str) return 0;
    sscanf(str, "%d", &ret);
    return ret;
}
/*
 * return 1 if v1 == v2
 */
int equal(const char* in, int own) {
    return char2int(in) == own;
}
/*
 * Convertors. We got only 2 params, so make 4 functions and that's all
*/
/* limits got structure as number,..,number,INT_MAX*/
static int ic(const char* in_value, rule_t rule, int limits[]) {
    int v = char2int(in_value);

    if(rule == rmin_max) {
        if(v < limits[0]) return limits[0];
        if(v > limits[1]) return limits[1];
        return v;
    }
/* list of values if we're here */
    int i = 0;
    while(limits[i] < INT_MAX) {
        if(v <= limits[i]) return limits[i];
        if(v >= limits[i+1]) {
            i++;
            continue;      /* let's check next pair */
        }
        /* Case between */
        int l_pos = v - limits[i];
        int r_pos = limits[i+1] - v;
        if(r_pos <= l_pos) return limits[i+1];
        else return limits[i];
    }
    return v;
}
/* cloud [0..100], cam [0..9] */
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

/*
 * Return string {"params_array":[{"name":"<name>", "value":"<value>"}, ...]} or NULL
 */
char* get_persistent_data(const char* file_name) {
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
 * Return IMDB index if found or -1 if not
 * serch by cam_name filed
 * Not sure its needed...
 * NB! add not found into log!!
 */
static find_camera_param(const char* cam_name) {
    int i;
    if(!cam_name) return -1;
    for(i = 0; i < NELEMS(SCHEME); i++) {
        if(!IMDB[i].cam_name) continue;
        if(!strcmp(IMDB[i].cam_name, cam_name)) return i;
    }
    pu_log(LL_WARNING, "%s Parameter %s not found in dB", __FUNCTION__, cam_name);
    return -1;
}
/*
 * Set change_flag & updated flags
 */
static void replace_param_value(int idx, const char* value) {
    if(!value) return;

    int val = char2int(value);
    if(val == IMDB[idx].value) return;

    IMDB[idx].value = val;
    IMDB[idx].change_flag = 1;
    if(!IMDB[idx].in_changes_report) IMDB[idx].updated = 1;
}

static int* copyLimits(const int from[]){
    size_t sz=0;
    while(from[sz++] < INT_MAX);
    int* ret= calloc(sz, sizeof(int));
    memcpy(ret, from, sz);
    return ret;
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
/*05*/  IMDB[i].in_changes_report = SCHEME[i].in_changes_report;
/*06*/  /* updated = 0 */
/*07*/  IMDB[i].persistent = SCHEME[i].persistent;
/*08*/  if(SCHEME[i].cam_name) {IMDB[i].cam_name = strdup(SCHEME[i].cam_name); ANAL(IMDB[i].cam_name);}
/*09*/  /* default_value not needed and not used */
/*10*/  IMDB[i].input_converter = SCHEME[i].input_converter;
/*11*/  IMDB[i].cloud_cam_converter = SCHEME[i].cloud_cam_converter;
/*12*/  IMDB[i].cam_cloud_converter = SCHEME[i].cam_cloud_converter;
/*13*/  IMDB[i].rule = SCHEME[i].rule;
/*14*/  if(SCHEME[i].limits) IMDB[i].limits = copyLimits(SCHEME[i].limits);
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
    if(!JSON_string) goto on_error;

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
 */
static int sync_camera_param(int idx) {
    if(!IMDB[idx].cam_name) return 1;     /* This parameter hasn't cam analogue */
    if(!IMDB[idx].cam_cloud_converter || !IMDB[idx].cloud_cam_converter) {
        pu_log(LL_ERROR, "%s: data error! conveter functions are unavailable!", __FUNCTION__);
        return 0;
    }
    int cam_value = IMDB[idx].cloud_cam_converter(IMDB[idx].value);
    int from_cam_value = ac_update_cam_parameter(IMDB[idx].cam_name, cam_value);
    int new_cloud_value = IMDB[idx].cam_cloud_converter(from_cam_value); 
    char buf[10] = {0};
    snprintf(buf, sizeof(buf)-1, "%d", new_cloud_value);
    replace_param_value(idx, buf);

    return 1;
}
/*
 * 1. Initialize camera data: upload cam parameters from camera
 * 2. Set all cam_parameter to IMDB values
 */
static int sync_camera_data() {
    int i;
    if(!ac_cam_init()) {
        pu_log(LL_ERROR, "%s, Error Camera initiation", __FUNCTION__);
        return 0;
    }
    for(i = 0; i < NELEMS(SCHEME); i++) {
        if(!sync_camera_param(i)) return 0;
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
    if(!load_persistent_data()) {
        pu_log(LL_ERROR, "%s: Error load persistent data", __FUNCTION__);
        ag_db_unload_cam_properties();
        return 0;
    }
    pu_log(LL_DEBUG, "%s: Persistent data loaded OK", __FUNCTION__);
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
    ac_cam_deinit();
    if(!IMDB) return;
    int i;
    for(i = 0; i < NELEMS(SCHEME); i++) {
        FREE(IMDB[i].name);
        FREE(IMDB[i].cam_name);
        FREE(IMDB[i].limits);
    }
    FREE(IMDB);
};

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
        if(IMDB[i].in_changes_report && IMDB[i].change_flag) add_reported_property(rep, IMDB[i].name, IMDB[i].value);
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
    FREE(rep);
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
 * !Set the property's flag ON in any case!.
 */
int ag_db_store_property(const char* property_name, const char* property_value) {
    int ret=0;
    int pos = find_param(property_name);
    if(pos < 0) return 0;
    if(!equal(property_value, IMDB[pos].value)) {
        replace_param_value(pos, property_value);
        if(IMDB[pos].cam_name) sync_camera_param(pos);
        ret = 1;
    }
    return ret;
}

int ag_db_get_int_property(const char* property_name) {
    int pos = find_param(property_name);
    if(pos < 0) return 0;

    return IMDB[pos].value;
}




