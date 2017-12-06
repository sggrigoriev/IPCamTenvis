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
 Coding and decoding camera messages to/from internal presentation
*/

#ifndef IPCAMTENVIS_AO_CMA_CAM_H
#define IPCAMTENVIS_AO_CMA_CAM_H

#include <stddef.h>

//#include "ac_rtsp.h"
#include "ao_cmd_data.h"

typedef enum {
    AO_RES_UNDEF,
    AO_RES_LO,
    AO_RES_HI
} t_ao_cam_res;

t_ac_rtsp_type ao_get_msg_type(const char* msg);
int ao_get_msg_number(const char* msg);
void ao_get_uri(char* uri, size_t size, const char* msg);
void ao_cam_replace_uri(char* msg, size_t size, const char* new_uri);
int ao_get_client_port(const char* msg);
int ao_get_server_port(const char* msg);

int ao_cam_encode(t_ao_msg data, const char* to_cam_msg, size_t size);


#endif /* IPCAMTENVIS_AO_CMA_CAM_H */
