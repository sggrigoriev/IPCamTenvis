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
 Contains internal presentation for all cloud, Proxy & camera messages
*/

#ifndef IPCAMTENVIS_AO_CMD_DATA_H
#define IPCAMTENVIS_AO_CMD_DATA_H

#include "lib_http.h"
#include "ag_defaults.h"

/*************************************************************************
 * All from and to Agent messages internak format: from Proxy, from Cam
 */

typedef enum {
    AO_UNDEF,
    AO_IN_PROXY_ID,             /* Obsolete. Proxy device ID - the command feft for compatibility with M-3 agent*/
    AO_IN_CONNECTION_STATE,     /* Off line or on line */
    AO_VIDEO_START,             /* Command to start/stop video broadcusting */

    AO_IN_PZT,                   /* PZT command - ot implemented */
} t_ao_msg_type;

typedef struct {
    char url[LIB_HTTP_MAX_URL_SIZE];
    int port;
    char auth[LIB_HTTP_AUTHENTICATION_STRING_SIZE];   /* Session ID in out case */
} t_ao_conn;

/* AO_IN_CONNECTION_STATE */
typedef struct {
    t_ao_msg_type   msg_type;
    char            proxy_device_id[LIB_HTTP_DEVICE_ID_SIZE];
    int             is_online;        /* 0 - ofline, 1 - online */
    char            proxy_auth[LIB_HTTP_AUTHENTICATION_STRING_SIZE];
    char            main_url[LIB_HTTP_MAX_URL_SIZE];
} t_ao_in_connection_state;

/* AO_VIDEO_START */
typedef struct {
    t_ao_msg_type   msg_type;
    int             start;        /* 0 - stop, 1 - start */
} t_ao_in_video_start;

/* AO_IN_PZT */
typedef struct {
    t_ao_msg_type    msg_type;
} t_ao_in_pzt;

typedef union {
    t_ao_msg_type               command_type;
    t_ao_in_connection_state    in_connection_state;
    t_ao_in_video_start         in_start_video;
    t_ao_in_pzt                 in_pzt;
} t_ao_msg;

#endif /* IPCAMTENVIS_AO_CMD_DATA_H */
