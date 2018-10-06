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
*/

#ifndef IPCAMTENVIS_AO_CMD_PROXY_H
#define IPCAMTENVIS_AO_CMD_PROXY_H

#include "pr_commands.h"

#include "ao_cmd_data.h"
/*
 * Returns 1 if there is "status":"ACK"
 */
int ao_proxy_ack_required(msg_obj_t* obj);
/*
 * returns command number or 0
 */
int ao_proxy_get_cmd_no(msg_obj_t* command);
/*
 * Returns "parameters" array
 */
msg_obj_t* ao_proxy_get_cloud_params_array(msg_obj_t* command);
/*
 * Returns param name or value from parameters array Ith item
 */
const char* ao_proxy_get_cloud_param_name(msg_obj_t* param);
const char* ao_proxy_get_cloud_param_value(msg_obj_t* param);

/*
 * Returns "params":[] object
 */
msg_obj_t* ao_proxy_get_ws_params_array(msg_obj_t* msg);
/*
 * Returns "name" or "setValue" from params Ith element
 */
const char* ao_proxy_get_ws_param_name(msg_obj_t* param);
const char* ao_proxy_get_ws_param_value(msg_obj_t* param);

void ao_proxy_decode(msg_obj_t* own_msg, t_ao_msg* data);

#endif /* IPCAMTENVIS_AO_CMD_PROXY_H */
