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

#include "ao_cmd_data.h"

/*******************************************************************************
 * Decode cloud/Proxy JSON message into internal structure (ao_cmd_data.h)
 * @param cloud_message - zero-terminated JSON string
 * @param data - internal structure
 * @return - message type
 */
t_ao_msg_type ao_cloud_decode(const char* cloud_message, t_ao_msg* data);


/*
 * Returns {"params":[{"name":"ppc.streamStatus","value":"2dgkaMa8b1RhLlr2cycqStJeU"}]}
 */
const char* ao_stream_approve(char* buf, size_t size, const char* session_id);

/*
 * Returns {"sessionId":"2dgkaMa8b1RhLlr2cycqStJeU"}
 */
const char* ao_connection_request(char* buf, size_t size, const char* session_id);
/*
 * Returns {"sessionId":"2dgkaMa8b1RhLlr2cycqStJeU", "requestViewers":true}
 */
const char* ao_active_viwers_request(char* buf, size_t size, const char* session_id);
/*
 * Returns {"responses": [{"commandId": <command_id> "result": <rc>}]}
 */
const char* ao_answer_to_command(char *buf, size_t size, int command_id, int rc);
/*
 * Returns "{}"
 */
const char* ao_answer_to_ws_ping();
/*
 * Returns {"resultCode":<AO_WS_THREAD_ERROR>}
 */
const char* ao_ws_error_answer(char* buf, size_t size);
/*
 * Returns {"resultCode":<AO_WS_TO_ERROR>}
 */
const char* ao_ws_to_error_answer(char* buf, size_t size);
/* Returns {"resultCode":<AO_RW_THREAD_ERROR>}
 *
 */
const char* ao_rw_error_answer(char* buf, size_t size);
/*
 * Decode clod RC
 */
const char* ao_ws_error(int rc);

#endif /* IPCAMTENVIS_AO_CMD_CLOUD_H */
