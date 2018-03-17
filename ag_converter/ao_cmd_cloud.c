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
#define AO_WS_THREAD_ERROR -23
#define AO_RW_THREAD_ERROR -24
#define AO_WS_TO_ERROR -25

static const char* CLOUD_RC = "resultCode";
static const char* CLOUD_PARAMS_ARR = "params";
static const char* CLOUD_SET_VALUE = "setValue";
static const char* CLOUD_VIEWERS = "viewers";
static const char* CLOUD_V_STATUS = "status";
static const char* CLOUD_VIEWERS_COUNT = "viewersCount";

/*
 * "params":[{"name":"ppc.streamStatus","setValue":"1","forward":0}]
*/
static int get_start_section(cJSON* obj) {
    cJSON* arr;
    cJSON* arr_item;
    cJSON* item;

    if(arr = cJSON_GetObjectItem(obj, CLOUD_PARAMS_ARR), (!arr || arr->type != cJSON_Array) || (cJSON_GetArraySize(arr) < 1)) return 0;
    arr_item = cJSON_GetArrayItem(arr, 0);
    if(item = cJSON_GetObjectItem(arr_item, CLOUD_SET_VALUE), !item) return 0;
    if(strcmp(item->valuestring, "1") != 0) return 0;

    return 1;
}
/*
 * "viewers":[{"id":"<id>","status":<>}]}
 */
static int get_viewers_section(cJSON* obj) {
    cJSON* arr;
    cJSON* arr_item;
    cJSON* item;
    int ret = 0, i;

    if(arr = cJSON_GetObjectItem(obj, CLOUD_VIEWERS), (!arr || arr->type != cJSON_Array) || (cJSON_GetArraySize(arr) < 1)) return ret;
    for(i = 0; i < cJSON_GetArraySize(arr); i++) {
        arr_item = cJSON_GetArrayItem(arr, i);
        if(item = cJSON_GetObjectItem(arr_item, CLOUD_V_STATUS), !item) return ret;
        if(item->type != cJSON_Number) return ret;
        ret += item->valueint;
    }
    return ret;
}
/*
 * "viewersCount":0
 */
static int get_count_section(cJSON* obj) {
    cJSON* item;
    int ret = -1;

    if(item = cJSON_GetObjectItem(obj, CLOUD_VIEWERS_COUNT), !item) return ret;
    if(item->type != cJSON_Number) return ret;
    ret = (item->valueint < 0)?0:item->valueint;
    return ret;
}
/*
 * {"resultCode":0,"params":[{"name":"ppc.streamStatus","setValue":"1","forward":0}],"viewers":[{"id":"24","status":1}], "viewersCount":0}
 */
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
    data->ws_answer.viwers_count = -1;
    data->ws_answer.viewers_delta = 0;
    data->ws_answer.is_start = 0;

    if(data->ws_answer.rc == AO_WS_PING_RC) {       // Ping
        data->ws_answer.ws_msg_type = AO_WS_PING;
        goto on_finish;
    }
    if(data->ws_answer.rc != 0) {                   //Error - if not a ping and not a zero
        data->ws_answer.ws_msg_type = AO_WS_ERROR;
        goto on_finish;
    }
    data->ws_answer.ws_msg_type = AO_WS_ABOUT_STREAMING;
    data->ws_answer.is_start = get_start_section(obj);
    data->ws_answer.viewers_delta = get_viewers_section(obj);
    data->ws_answer.viwers_count = get_count_section(obj);

on_finish:
    cJSON_Delete(obj);
    return data->command_type;
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
 * Returns {"sessionId":"<sessionID>", "params":[], "requestViewers":true, "pingType":2}
 */
const char* ao_connection_request(char* buf, size_t size, const char* session_id) {
    const char* part1 = "{\"sessionId\":\"";
    const char* part2 = "\", \"params\":[], \"requestViewers\":true, \"pingType\":2}";

    snprintf(buf, size-1, "%s%s%s", part1, session_id, part2);
    return buf;
}

/*
 * {"responses": [{"commandId": <command_id> "result": <rc>}]}
 */
const char* ao_answer_to_command(char *buf, size_t size, int command_id, int rc) {
    const char* part1 = "{\"responses\": [{\"commandId\": ";
    const char* part2 = " \"result\": ";
    const char* part3 = "}]}";

    snprintf(buf, size-1, "%s%d%s%d%s", part1, command_id, part2, rc, part3);
    buf[size-1] = '\0';
    return buf;
}
/*
 * Returns "{}"
 */
const char* ao_answer_to_ws_ping() {
    const char* part1 = "{}";
    return part1;
}
/*
 * Returns {"resultCode":<own_error>}
 */
static char* own_error_answer(char* buf, size_t size, int err) {
    const char* part1 = "{\"resultCode\": ";
    const char* part2 = "}";

    snprintf(buf, size-1, "%s%d%s", part1, err, part2);
    buf[size-1] = '\0';
    return buf;
}
const char* ao_ws_error_answer(char* buf, size_t size) {
    return own_error_answer(buf, size, AO_WS_THREAD_ERROR);
}
const char* ao_ws_to_error_answer(char* buf, size_t size) {
    return own_error_answer(buf, size, AO_WS_TO_ERROR);
}
const char* ao_rw_error_answer(char* buf, size_t size) {
    return own_error_answer(buf, size, AO_RW_THREAD_ERROR);
}

/*******************************************************************************************************
 * Cloud Web Socket RC handling
*/
#define AO_WS_DIAGNOSTICS_AMOUNT 15

typedef struct {
    int rc;
    const char* diagnostics;
} t_ao_ws_diagnostics;

static const t_ao_ws_diagnostics ws_diagnostics[AO_WS_DIAGNOSTICS_AMOUNT] = {
        {AO_RW_THREAD_ERROR, "Streaming R/W treads error"},
        {AO_WS_THREAD_ERROR, "WS thread internal error"},
        {AO_WS_TO_ERROR, "No pings from Web Socket"},
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


