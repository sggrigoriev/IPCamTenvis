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
 Created by gsg on 09/12/17.
*/
#include <string.h>
#include "au_string.h"

#include "cJSON.h"
#include "pu_logger.h"

#include "ac_cam.h"
#include "ao_cmd_data.h"

#include "ao_cmd_proxy.h"


static const char* F_DEVICE_ID_OLD = "gw_gatewayDeviceId";
static const char* F_DEVICE_ID = "deviceId";
static const char* F_CONN_STATE = "cloudConnection";
static const char* F_AUTH_TOKEN = "deviceAuthToken";
static const char* F_CONN_STRING = "connString";

static const char* V_CONNECTED = "connected";
static const char* V_DISCONNECTED = "disconnected";

static const char* F_COMMAND_ARRAY = "commands";
static const char* F_COMMAND_ID = "commandId";
static const char* F_PARAMS_ARRAY = "parameters";
static const char* F_PARAM_NAME = "name";
static const char* F_STREAM_NAME = "ppc.streamStatus";
static const char* F_PARAM_VALUE = "value";
static const char* F_VALUE_START = "1";
static const char* F_VALUE_STOP = "0";

/*
 * Returns 1 if the object got {"status":"ACK",...
 */
int ao_proxy_ack_required(msg_obj_t* obj) {
    cJSON* item = cJSON_GetObjectItem(obj, "status");
    if(!item) return 0;
    return(!strcmp("ACK", item->valuestring));
}
/*
 * Return "commandId" value from "commands" array Ith item.
 */
int ao_proxy_get_cmd_no(msg_obj_t* command) {
    cJSON* item = cJSON_GetObjectItem(command, "commandId");
    if(!item) return 0;
    return item->valueint;
}
/*
 * Returns "parameters":[] array
 */
msg_obj_t* ao_proxy_get_cloud_params_array(msg_obj_t* command) {
    return cJSON_GetObjectItem(command, "parameters");
}
/*
 * Returns param name or value from parameters array Ith item
 * {"name":"<par_name>","value":"<par_value>"}
 */
const char* ao_proxy_get_cloud_param_name(msg_obj_t* param) {
    cJSON* par = cJSON_GetObjectItem(param, "name");
    if(!par) return NULL;
    return par->valuestring;
}
const char* ao_proxy_get_cloud_param_value(msg_obj_t* param) {
    cJSON* par = cJSON_GetObjectItem(param, "value");
    if(!par) return NULL;
    return par->valuestring;
}
/*
 * Returns "<arr_name>":[] object
 */
msg_obj_t* ao_proxy_get_ws_array(msg_obj_t* msg, const char* arr_name) {
    msg_obj_t* ret = cJSON_GetObjectItem(msg, arr_name);
    return (!ret || (ret->type != cJSON_Array))?NULL:ret;
}
/*
 * Returns "name" or "setValue" from params Ith element
 */
const char* ao_proxy_get_ws_param_name(msg_obj_t* param) {
    cJSON* par = cJSON_GetObjectItem(param, "name");
    if(!par) return NULL;
    return par->valuestring;
}
const char* ao_proxy_get_ws_param_value(msg_obj_t* param) {
    cJSON* par = cJSON_GetObjectItem(param, "setValue");
    if(!par) return NULL;
    return par->valuestring;
}


/* {gw_gatewayDeviceId":[{"paramsMap":{"deviceId":"<proxy_device_id>"}}]} */
static int get_old_proxy_msg_with_device_id(cJSON* data) {
    cJSON* item = cJSON_GetObjectItem(data, F_DEVICE_ID_OLD);
    return (item != NULL);
}
/*
 * 		{"commands": [
			{"deviceId":"<deviceID>", "type":0, "parameters": [
				{"name":"cloudConnection","value":"<connected/disconnected>"},
				{"name":"deviceAuthToken","value":"<auth_token>"},
				{"name":"connString","value":"<Main_Cloud_URL>"}
			]}
		]}
*/
static int get_proxy_conn_status(cJSON* data, t_ao_in_connection_state* fld) {
    cJSON* arr;
    cJSON* arr_item;
    cJSON* p_arr;
    cJSON* ent;

    if(arr = cJSON_GetObjectItem(data, F_COMMAND_ARRAY), (!arr || arr->type != cJSON_Array) || (cJSON_GetArraySize(arr) < 1)) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_COMMAND_ARRAY);
        return 0;
    }
    arr_item = cJSON_GetArrayItem(arr, 0);
    if(ent = cJSON_GetObjectItem(arr_item, F_DEVICE_ID), !ent) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_DEVICE_ID);
        return 0;
    }
    if(!au_strcpy(fld->proxy_device_id, ent->valuestring, sizeof(fld->proxy_device_id))) return 0;

    if(p_arr = cJSON_GetObjectItem(arr_item, F_PARAMS_ARRAY), !p_arr) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_PARAMS_ARRAY);
        return 0;
    }
    int i;
    for(i = 0; i < cJSON_GetArraySize(p_arr); i++) {
        ent = cJSON_GetArrayItem(p_arr, i);
        cJSON* ent_name = cJSON_GetObjectItem(ent, F_PARAM_NAME);
        cJSON* ent_val = cJSON_GetObjectItem(ent, F_PARAM_VALUE);
        if(!ent||!ent_name||!ent_val||(ent_name->type!=cJSON_String)||(ent_val->type!=cJSON_String)) {
            pu_log(LL_ERROR, "%s: ConnInfo message expected from Proxy. Message ignored", __FUNCTION__);
            return 0;
        }
        if(!strcmp(ent_name->valuestring, F_CONN_STATE)) {
            if(!strcmp(ent_val->valuestring, V_CONNECTED))
                fld->is_online = 1;
            else if(!strcmp(ent_val->valuestring, V_DISCONNECTED))
                fld->is_online = 0;
            else {
                pu_log(LL_ERROR, "%s: '%s' value %s/%s for field %s is not found", __FUNCTION__, V_CONNECTED, V_DISCONNECTED, F_CONN_STATE);
                return 0;
            }
        }
        else if(!strcmp(ent_name->valuestring, F_AUTH_TOKEN)) {
            if(!au_strcpy(fld->proxy_auth, ent_val->valuestring, sizeof(fld->proxy_auth))) return 0;
        }
        else if(!strcmp(ent_name->valuestring, F_CONN_STRING)) {
            if(!au_strcpy(fld->main_url, ent_val->valuestring, sizeof(fld->main_url))) return 0;
        }
    }
    return 1;
}


static int get_stream_start_stop(cJSON* obj, t_ao_in_manage_video* data) {
    cJSON* arr;
    cJSON* arr_item;
    cJSON* item;
    int command_id;

    if(arr = cJSON_GetObjectItem(obj, F_COMMAND_ARRAY), (!arr || arr->type != cJSON_Array) || (cJSON_GetArraySize(arr) < 1)) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_COMMAND_ARRAY);
        return 0;
    }
    arr_item = cJSON_GetArrayItem(arr, 0);
    if(item = cJSON_GetObjectItem(arr_item, F_COMMAND_ID), !item) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_COMMAND_ID);
        return 0;
    }
    command_id = item->valueint;

    if(arr = cJSON_GetObjectItem(arr_item, F_PARAMS_ARRAY), (!arr || arr->type != cJSON_Array) || (cJSON_GetArraySize(arr) < 1)) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_PARAMS_ARRAY);
        return 0;
    }
    arr_item = cJSON_GetArrayItem(arr, 0);
    if(item = cJSON_GetObjectItem(arr_item, F_PARAM_NAME), !item) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_PARAM_NAME);
        return 0;
    }
    if(strcmp(item->valuestring, F_STREAM_NAME) != 0) {
        pu_log(LL_ERROR, "%s: '%s' field has wrong value %s. Value %s expected", __FUNCTION__, F_PARAM_NAME, item->valuestring, F_STREAM_NAME);
        return 0;
    }
    if(item = cJSON_GetObjectItem(arr_item, F_PARAM_VALUE), !item) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_PARAM_VALUE);
        return 0;
    }
    if(item->type != cJSON_String) {
        pu_log(LL_ERROR, "%s: '%s' field value is not a string.", __FUNCTION__, F_PARAM_VALUE);
        return 0;
    }
    if(strcmp(item->valuestring, F_VALUE_START) == 0) {
        data->start_it = 1;
    }
    else if(strcmp(item->valuestring, F_VALUE_STOP) == 0) {
        data->start_it = 0;
    }
    else {
        pu_log(LL_ERROR, "%s: '%s' field got unexpected value %s. Only %s or %s are supported.", __FUNCTION__, F_PARAM_VALUE, item->valuestring, F_VALUE_START, F_VALUE_STOP);
        return 0;
    }
    data->msg_type = AO_IN_MANAGE_VIDEO;
    data->command_id = command_id;

    return 0;
}

void ao_proxy_decode(msg_obj_t* own_msg, t_ao_msg* data) {
    data->command_type = AO_UNDEF;

    if(get_old_proxy_msg_with_device_id(own_msg)) {
        data->command_type = AO_IN_PROXY_ID;
    }
    else if(get_proxy_conn_status(own_msg, &data->in_connection_state)) {
        data->command_type = AO_IN_CONNECTION_INFO;
    }
    else {
        get_stream_start_stop(own_msg, &data->in_manage_video);
    }
}
