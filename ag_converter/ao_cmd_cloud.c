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
*/
#include <string.h>

#include "cJSON.h"

#include "pu_logger.h"

#include "ao_cmd_data.h"
#include "ao_cmd_cloud.h"

#define AO_WS_PING_RC  10

static const char* CLOUD_RC = "resultCode";
static const char* CLOUD_PARAMS_ARR = "params";
static const char* CLOUD_SET_VALUE = "setValue";
static const char* CLOUD_VIEWERS_COUNT = "viewersCount";

/*
 * "params":[{"name":"ppc.streamStatus","setValue":"1","forward":0}]
*/
static int recognize_start(cJSON* obj, t_ao_msg* data) {
    cJSON* arr;
    cJSON* arr_item;
    cJSON* item;

    if(arr = cJSON_GetObjectItem(obj, CLOUD_PARAMS_ARR), (!arr || arr->type != cJSON_Array) || (cJSON_GetArraySize(arr) < 1)) return 0;
    arr_item = cJSON_GetArrayItem(arr, 0);
    if(item = cJSON_GetObjectItem(arr_item, CLOUD_SET_VALUE), !item) return 0;
    if(strcmp(item->valuestring, "1") != 0) return 0;

    data->ws_answer.ws_msg_type = AO_WS_START;
    return 1;
}
/*
 * "viewersCount":0
 */
static int recognize_stop(cJSON* obj, t_ao_msg* data) {
    cJSON* item;

    if(item = cJSON_GetObjectItem(obj, CLOUD_VIEWERS_COUNT), !item) return 0;
    if(item->valueint != 0) return 0;

    data->ws_answer.ws_msg_type = AO_WS_STOP;
    return 1;
}

t_ao_msg_type ao_cloud_decode(const char* cloud_message, t_ao_msg* data) {
    cJSON* obj;
    cJSON* item;

    data->command_type = AO_UNDEF;

    if(obj = cJSON_Parse(cloud_message), !obj) {
        pu_log(LL_ERROR, "%s: Error parsing %s", __FUNCTION__, cloud_message);
        return data->command_type;
    }
    if(item = cJSON_GetObjectItem(obj, CLOUD_RC), !item) {
        pu_log(LL_ERROR, "%s: Field %s not found in message %s", __FUNCTION__, CLOUD_RC, cloud_message);
        goto on_finish;
    }
    if(item->type != cJSON_Number) {
        pu_log(LL_ERROR, "%s: Field %s has wromg type in message %s. Numeric type expected.", __FUNCTION__, CLOUD_RC, cloud_message);
        goto on_finish;
    }
    data->command_type = AO_WS_ANSWER;
    data->ws_answer.rc = item->valueint;

    if(data->ws_answer.rc == AO_WS_PING_RC) {       // Ping
        data->ws_answer.ws_msg_type = AO_WS_PING;
        goto on_finish;
    }
    if(data->ws_answer.rc != 0) {                   //Error
        data->ws_answer.ws_msg_type = AO_WS_ERROR;
        goto on_finish;
    }

    if(recognize_start(obj, data)) goto on_finish;  //"params":[{"name":"ppc.streamStatus","setValue":"1","forward":0}]
    if(recognize_stop(obj, data)) goto on_finish;   //"viewersCount":0

    data->ws_answer.ws_msg_type = AO_WS_NOT_INTERESTING;

on_finish:
    cJSON_Delete(obj);
    return data->command_type;
}

/*
 * Returns {"sessionId":"2dgkaMa8b1RhLlr2cycqStJeU"}
 */
const char* ao_stream_request(char* buf, size_t size, const char* session_id) {
    const char* part1 = "{\"sessionId\":\"";
    const char* part2 = "\"}";
    snprintf(buf, size-1, "%s%s%s", part1, session_id, part2);
    return buf;
}

/*
 * Returns {"params":[{"name":"ppc.streamStatus","value":"2dgkaMa8b1RhLlr2cycqStJeU"}]}
 */
const char* ao_stream_approve(char* buf, size_t size, const char* session_id) {
    const char* part1 = "{\"params\":[{\"name\":\"ppc.streamStatus\",\"value\":\"";
    const char* part2 = "\"}]}";
    snprintf(buf, size-1, "%s%s%s", part1, session_id, part2);
    return buf;
}

/*
 * Returns {"sessionId":"2dgkaMa8b1RhLlr2cycqStJeU"}
 */
const char* ao_connection_request(char* buf, size_t size, const char* session_id) {
    const char* part1 = "{\"sessionId\":\"";
    const char* part2 = "\"}";
    snprintf(buf, size-1, "%s%s%s", part1, session_id, part2);
    return buf;
}

/*******************************************************************************************************
 * Cloud Web Socket RC handling
*/
#define AO_WS_DIAGNOSTICS_AMOUNT 12

typedef struct {
    int rc;
    const char* diagnostics;
} t_ao_ws_diagnostics;

static const t_ao_ws_diagnostics ws_diagnostics[AO_WS_DIAGNOSTICS_AMOUNT] = {
        {0, "successful"},
        {1, "internal error"},
        {2, "wrong API key"},
        {3, "wrong device authentication token"},
        {4, "wrong device ID or device has not been found"},
        {5, "wrong session ID"},
        {6, "camera not connected"},
        {7, "camera not connected"},
        {8, "wrong parameter value"},
        {9, "missed mandatory parameter value"},
        {AO_WS_PING_RC, "ping"},
        {30, "service is temporary unavailable"}
};

static const char* find_text(int rc) {
    int i;
    for(i = 0; i < AO_WS_DIAGNOSTICS_AMOUNT; i++) if(ws_diagnostics[i].rc == rc) return ws_diagnostics[i].diagnostics;
    return NULL;
}

const char* ao_ws_error(int rc) {
    const char* ret = find_text(rc);
    return (ret)?ret:"Unrecognized RC from Web Socket";


}


