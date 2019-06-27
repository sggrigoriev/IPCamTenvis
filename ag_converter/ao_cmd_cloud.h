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
 Coding and decoding cloud & Proxy messages to/from internal presentation
*/

#ifndef IPCAMTENVIS_AO_CMD_CLOUD_H
#define IPCAMTENVIS_AO_CMD_CLOUD_H

#include <stddef.h>

#include "cJSON.h"

#include "ao_cmd_data.h"

/* Some RCs */
#define AO_WS_PING_RC  10       /* Answer to cloud's ping to WS */
#define AO_WS_THREAD_ERROR -23  /* Web Socket thread error */
#define AO_RW_THREAD_ERROR -24  /* Streaming thread error */
#define AO_WS_TO_ERROR -25      /* No pings from WS on cloud side */


/**
 * Make answer for command execution as [{"commandId": <command_id> "result": <rc>}]
 *
 * @param command_id    - cloud commandID
 * @param rc            - execution RC
 * @return  - JSON structure with the answer created.
 */
cJSON* ao_cmd_cloud_responses(int command_id, int rc);
/**
 * Create the alert message to the cloud regarding camera event happens as
 * {"alerts": [
 *    {"alertId": "<alert_number>", "deviceId": "device_id", "alertType": "<motion/audio>", "timesec": <time_in_seconds>,
 *      "params": [					-- will not be presented if no file uploaded!!
 *          {"name": "fileRef", "value": "<file_ref_number>" }
 *      ]
 *   }
 * ]}
 * NB! returned memory should be freed!
 *
 * @param deviceID      - camera device ID
 * @param ev            - event type (see ao_cmd_data.h)
 * @param event_time    - event Unix time in seconds
 * @param fileRef       - pointer to the fileRef. NULL if no file reference
 * @return  - prepared JSON
 */
cJSON* ao_cmd_cloud_alerts(const char* deviceID, t_ac_cam_events ev, time_t event_time, const char* fileRef);

/**
 * Create cJSON array object like
 * [{"params":[<report>], "deviceId":"<deviceID>"}]
 * NB! returned memory should be freed!
 *
 * @param report    - report. JSON as [{"name":"<ParameterName>", "value":"<ParameterValue"}, ...]
 * @param deviceID  - camera device id
 * @return  - array object with report inserted
 */
cJSON* ao_cmd_cloud_measures(cJSON* report, const char* deviceID);

/**
 * Create the message like:
 * {"proxyId":"<deviceID>","seq":"<seq_number>", "alerts":<alerts>,"responses":<responses>,"measures":<measures>}
 *
 * @param deviceID  - camera device id
 * @param alerts    - alerts object or NULL if empty - empty array will be added
 * @param responses - responses object or NULL if empty - empty array will be added
 * @param measures  - measures object or NULL if empty - empty array will be added
 * @param buf       - buffer for the message constructed
 * @param size      - buffer size
 * @return  - pointer to the buffer or empty string if the buffer too small.
 */
const char* ao_cmd_cloud_msg(const char* deviceID, cJSON* alerts, cJSON* responses, cJSON* measures, char* buf, size_t size);

/************************ WS/streaming functions ***********************************************************
*/

/**
 * Decode cloud message as
 * {"resultCode":0,"params":[{"name":"ppc.streamStatus","setValue":"1","forward":0}],"viewers":[{"id":"24","status":1}], "viewersCount":0}
 *
 * @param cloud_message - cloud message
 * @param data          - decoded structure (see ao_cmd_data.h)
 * @return  - message type (see ao_cmd_data.h)
 */
t_ao_msg_type ao_cmd_cloud_decode(const char* cloud_message, t_ao_msg* data);

/*
 * Returns {"params":[{"name":"ppc.streamStatus","value":"2dgkaMa8b1RhLlr2cycqStJeU"}]}
 */
/**
 * Create the message approved the cloud request for streaming as
 * {"params":[{"name":"ppc.streamStatus","value":"<session ID>"}]}
 *
 * @param buf           - buffer to store the message
 * @param size          - buffer size
 * @param session_id    - WS session ID
 * @return  - pointer to the buffer
 */
const char* ao_cmd_cloud_stream_approve(char* buf, size_t size, const char* session_id);

/**
 * Create connection request ot WS interface as
 * {"sessionId":"<sessionID>", "params":[], "pingType":2}
 *
 * @param buf           - buffer to store the message
 * @param size          - buffer size
 * @param session_id    - WS session ID
 * @return  - pointer to the buffer
 */
const char* ao_cmd_cloud_connection_request(char* buf, size_t size, const char* session_id);

/**
 * Create error report to WS interface as
 * {"sessionId":"<sess_id>", "params":[{"name":"streamError","value":"<err_msg>"}]}
 *
 * @param err_msg   - error message
 * @param sessId    - WS session ID
 * @param buf           - buffer to store the message
 * @param size          - buffer size
 * @return  - pointer to the buffer
 */
const char* ao_cmd_cloud_stream_error_report(const char* err_msg, const char* sessId, char* buf, size_t size);

/**
 * Create current parameters value report to WS interface as
 * {"sessionId":"2kr51ar8x8jWD9YAf8ByOZKeW", "params":[{"name":"<param_name>","value":"<param_value>"},...]}
 *
 * @param report    - "params" array as JSON structure
 * @param buf       - buffer to store result
 * @param size      - buffer size
 * @return          - pointer to buffer or NULL if buffer too small
 */
const char* ao_cmd_ws_params(cJSON* report, char* buf, size_t size);

/**
 * Create Active Viewers request message for WS interface as
 * {"sessionId":"<seaaion id>", "requestViewers":true}
 *
 * @param sessionID - WS session id
 * @param buf       - buffer to store result
 * @param size      - buffer size
 * @return  - poiner to the buffer
 */
const char* ao_cmd_ws_active_viewers_request(const char* sessionID, char* buf, size_t size);

/**
 * Create answer to ping message for WS interface as
 * "{}"
 *
 * @return  - pointer to the constant string
 */
const char* ao_cmd_ws_answer_to_ping();

/**
 * Create error message to WS interface as
 * {"resultCode": <AO_WS_THREAD_ERROR>}
 *
 *
 * @param buf   - buffer to store the result
 * @param size  - buffer size
 * @return  - poiner to the buffer
 */
const char* ao_cmd_ws_error_answer(char* buf, size_t size);

/**
 * Create streaming thread(s) error message for WS interface
 *
 * @param buf   - buffer to store result
 * @param size  - buffer size
 * @return  - pointer to the buffer
 */
const char* ao_cmd_rw_error_answer(char* buf, size_t size);

#endif /* IPCAMTENVIS_AO_CMD_CLOUD_H */
