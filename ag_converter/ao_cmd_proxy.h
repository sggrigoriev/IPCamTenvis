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
 Decode messages from Proxy process
 Should be redactored!
*/

#ifndef IPCAMTENVIS_AO_CMD_PROXY_H
#define IPCAMTENVIS_AO_CMD_PROXY_H

#include "pr_commands.h"

#include "ao_cmd_data.h"
/*
 * Returns 1 if there is "status":"ACK"
 */
/**
 * Check if JSON object got "status":"ACK"
 *
 * @param obj   - JSON object
 * @return  - 0 if not found, 1 if contains it
 */
int ao_proxy_ack_required(msg_obj_t* obj);
/*
 * returns command number or 0
 */
/**
 * Get the command number from the JSON object
 *
 * @param command   - JSON object
 * @return  - command number or 0 if command number not found
 * NB! Should be changed from int to long!
 */
int ao_proxy_get_cmd_no(msg_obj_t* command);

/**
 * Get the pointer to "parameters" array object for cloud interface
 *
 * @param command   - JSON object with the cloud/WS command
 * @return  - pointer to the object or NULL if not found
 * NB! Check to the array type should be added!
 */
msg_obj_t* ao_proxy_get_cloud_params_array(msg_obj_t* command);

/**
 * Get the pointer to the "name" object value for cloud interface
 *
 * @param param - JSON object
 * @return  - pointer to the string value or NULL if not found
 * NB! Check to the value type == string should be added!
 */
const char* ao_proxy_get_cloud_param_name(msg_obj_t* param);


/**
 * Get the pointer to the "value" object value for cloud interface
 *
 * @param param - JSON object
 * @return  - pointer to the string value or NULL if not found
 * NB! Check to the value type == string should be added!
 */
const char* ao_proxy_get_cloud_param_value(msg_obj_t* param);

/**
 * Get the pointer to the "<arr_name>":[] object for WS interface
 *
 * @param msg       - message as JSON
 * @param arr_name  - array name to get
 * @return  - poiner to the object or NULL if not found
 */
msg_obj_t* ao_proxy_get_ws_array(msg_obj_t* msg, const char* arr_name);

/**
 * Get the pointer to the "name" object value for WS interface
 *
 * @param param - JSON object
 * @return  - pointer to the object; NULL if not found
 */
const char* ao_proxy_get_ws_param_name(msg_obj_t* param);

/**
 * Get the pointer to the "valur" object value for WS interface
 *
 * @param param - JSON object
 * @return  - pointer to the object; NULL if not found
 */
const char* ao_proxy_get_ws_param_value(msg_obj_t* param);

/**
 * Decode proxy message to agent
 *
 * @param own_msg   - Proxy message as JSON
 * @param data      - pointer to decoded message (see ao_cmd_data.h)
 */
void ao_proxy_decode(msg_obj_t* own_msg, t_ao_msg* data);

#endif /* IPCAMTENVIS_AO_CMD_PROXY_H */