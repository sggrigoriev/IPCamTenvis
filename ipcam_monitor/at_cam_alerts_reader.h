/*
 *  Copyright 2018 People Power Company
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
 Created by gsg on 18/09/18.
*/

#ifndef IPCAMTENVIS_AT_CAM_ALERTS_READER_H
#define IPCAMTENVIS_AT_CAM_ALERTS_READER_H

typedef enum {
    MON_NAME=0,
    MON_AGENT_IP=1,
    MON_AGENT_PORT=2,
    MON_CAM_IP=3,
    MON_CAM_PORT=4,
    MON_CAM_LOGIN=5,
    MON_CAM_PASSWORD=6,
    MON_CONTACT_URL=7,
    MON_PROXY_ID=8,
    MON_AUTH_TOKEN=9,
    MON_SIZE
} mon_params_t;

typedef struct {
    char* process_name;
    char* agent_ip;
    int agent_port;
    char* cam_ip;
    int cam_port;
    char* cam_login;
    char* cam_password;
} input_params_t;

void at_mon_stop();

void at_mon_function(const input_params_t* params);

#endif /* IPCAMTENVIS_AT_CAM_ALERTS_READER_H */
