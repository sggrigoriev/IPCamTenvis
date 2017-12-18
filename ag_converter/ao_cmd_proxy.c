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
#include <au_string/au_string.h>

#include "cJSON.h"
#include "pu_logger.h"

#include "ao_cmd_data.h"

#include "ao_cmd_proxy.h"


static const char* F_CLOUD_CONN = "gw_cloudConnection";
static const char* F_DEVICE_ID_OLD = "gw_gatewayDeviceId";
static const char* F_DEVICE_ID = "deviceId";
static const char* F_PAR_MAP = "paramsMap";
static const char* F_CONN_STATE = "cloudConnection";
static const char* F_AUTH_TOKEN = "deviceAuthToken";
static const char* F_CONN_STRING = "connString";

static const char* V_CONNECTED = "connected";
static const char* V_DISCONNECTED = "disconnected";


/* {gw_gatewayDeviceId":[{"paramsMap":{"deviceId":"<proxy_device_id>"}}]} */
static int get_old_proxy_msg_with_device_id(cJSON* data) {
    cJSON* item = cJSON_GetObjectItem(data, F_DEVICE_ID_OLD);
    return (item != NULL);
}
/* {"gw_cloudConnection": [{"deviceId":"<gateway device id>", "paramsMap": {"cloudConnection": "<connected/disconnected>", "deviceAuthToken":"<auth_token>"}}]} */
static int get_proxy_conn_status(cJSON* data, t_ao_in_connection_state* fld) {
    cJSON* arr;
    cJSON* arr_item;
    cJSON* map;
    cJSON* ent;

    if(arr = cJSON_GetObjectItem(data, F_CLOUD_CONN), (!arr || arr->type != cJSON_Array) || (cJSON_GetArraySize(arr) < 1)) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_CLOUD_CONN);
        return 0;
    }
    arr_item = cJSON_GetArrayItem(arr, 0);
    if(ent = cJSON_GetObjectItem(arr_item, F_DEVICE_ID), !ent) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_DEVICE_ID);
        return 0;
    }
    if(!au_strcpy(fld->proxy_device_id, ent->valuestring, sizeof(fld->proxy_device_id))) return 0;

    if(map = cJSON_GetObjectItem(arr_item, F_PAR_MAP), !map) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_PAR_MAP);
        return 0;
    }

    if(ent = cJSON_GetObjectItem(map, F_CONN_STATE), !ent) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_CONN_STATE);
        return 0;
    }
    if(!strcmp(ent->valuestring, V_CONNECTED))
        fld->is_online = 1;
    else if(!strcmp(ent->valuestring, V_DISCONNECTED))
        fld->is_online = 0;
    else {
        pu_log(LL_ERROR, "%s: '%s' value %s/%s for field %s is not found", __FUNCTION__, V_CONNECTED, V_DISCONNECTED, F_CONN_STATE);
        return 0;
    }

    if(ent = cJSON_GetObjectItem(map, F_AUTH_TOKEN), !ent) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_AUTH_TOKEN);
        return 0;
    }
    if(!au_strcpy(fld->proxy_auth, ent->valuestring, sizeof(fld->proxy_auth))) return 0;

    if(ent = cJSON_GetObjectItem(map, F_CONN_STRING), !ent) {
        pu_log(LL_ERROR, "%s: '%s' field is not found", __FUNCTION__, F_CONN_STRING);
        return 0;
    }
    if(!au_strcpy(fld->main_url, ent->valuestring, sizeof(fld->main_url))) return 0;

    return 1;
}


t_ao_msg_type ao_proxy_decode(const char* msg, t_ao_msg* data) {
    cJSON* obj;

    data->command_type = AO_UNDEF;

    if(obj = cJSON_Parse(msg), !obj) {
        pu_log(LL_ERROR, "%s: Error parsing %s", msg);
        return data->command_type;
    }

    if(get_old_proxy_msg_with_device_id(obj))
        data->command_type = AO_IN_PROXY_ID;
    else if(get_proxy_conn_status(obj, &data->in_connection_state))
        data->command_type = AO_IN_CONNECTION_STATE;

    cJSON_Delete(obj);
    return data->command_type;
}
