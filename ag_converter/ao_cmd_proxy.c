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

#include "cJSON.h"

#include "ao_cmd_data.h"

#include "ao_cmd_proxy.h"


static const char* F_CLOUD_CONN = "gw_cloudConnection";
static const char* F_DEICE_ID_OLD = "gw_gatewayDeviceId";
static const char* F_DEVICE_ID = "deviceId";
static const char* F_PAR_MAP = "paramsMap";
static const char* F_CONN_STATE = "cloudConnection";
static const char* F_AUTH_TOKEN = "deviceAuthToken";
static const char* F_CONN_STRING = "connString";

static const char* V_CONNECTED = "connected";
static const char* V_DISCONNECTED = "disconnected";


/* {gw_gatewayDeviceId":[{"paramsMap":{"deviceId":"<proxy_device_id>"}}]} */
static int get_old_proxy_msg_with_device_id(cJSON* data) {
    cJSON* item = cJSON_GetObjectItem(data, F_DEICE_ID_OLD);
    return (item != NULL);
}
/* {"gw_cloudConnection": [{"deviceId":"<gateway device id>", "paramsMap": {"cloudConnection": "<connected/disconnected>", "deviceAuthToken":"<auth_token>"}}]} */
static int get_proxy_conn_status(cJSON* data, t_ao_in_connection_state* fld) {
    cJSON* arr;
    cJSON* arr_item;
    cJSON* par;
    cJSON* ent;

    if(arr = cJSON_GetObjectItem(data, F_CLOUD_CONN), (!arr || arr->type != cJSON_Array) || (cJSON_GetArraySize(arr) < 1)) return 0;
    arr_item = cJSON_GetArrayItem(arr, 0);
    if(par = cJSON_GetObjectItem(arr_item, F_DEVICE_ID), !par) return 0;
    strncpy(fld->proxy_device_id, par->string, sizeof(fld->proxy_device_id));

    if(par = cJSON_GetObjectItem(arr_item, F_DEVICE_ID), !par) return 0;
    strncpy(fld->proxy_device_id, par->string, sizeof(fld->proxy_device_id));

    if(par = cJSON_GetObjectItem(arr_item, F_PAR_MAP), !par) return 0;

    if(ent = cJSON_GetObjectItem(arr_item, F_CONN_STATE), !ent) return 0;
    if(!strcmp(ent->string, V_CONNECTED))
        fld->is_online = 1;
    else if(!strcmp(ent->string, V_DISCONNECTED))
        fld->is_online = 0;
    else return 0;

    if(ent = cJSON_GetObjectItem(arr_item, F_AUTH_TOKEN), !ent) return 0;
    strncpy(fld->proxy_auth, ent->string, sizeof(fld->proxy_auth));

    if(ent = cJSON_GetObjectItem(arr_item, F_CONN_STRING), !ent) return 0;
    strncpy(fld->main_url, ent->string, sizeof(fld->main_url));

    return 1;
}


t_ao_msg_type ao_proxy_decode(const char* msg, t_ao_msg* data) {
    cJSON* obj;

    data->command_type = AO_UNDEF;

    if(obj = cJSON_Parse(msg), !obj) return data->command_type;

    if(get_old_proxy_msg_with_device_id(obj))
        data->command_type = AO_IN_PROXY_ID;
    else if(get_proxy_conn_status(obj, &data->in_connection_state))
        data->command_type = AO_IN_CONNECTION_STATE;

    cJSON_Delete(obj);
    return data->command_type;
}
