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

/*************************************************************************
 * Cloud & Proxy messages
 */

typedef enum {
    AO_CLOUD_UNDEF,           /* Can't understand the command */
    AO_CLOUD_PROXY_ID,         /* Proxy device ID */
    AO_CLOUD_CONNECTION_STATE,/* Off line or on line */
    AO_CLOUD_VIDEO_START,     /* Start video streaming */
    AO_CLOUD_VIDEO_STOP,      /* Stop video straeming */
    AO_CLOUD_PZT              /* PZT command */
} t_ao_cloud_msg_type;

typedef struct{
    t_ao_cloud_msg_type    msg_type;
    char proxy_device_id[LIB_HTTP_DEVICE_ID_SIZE];
} t_ao_proxy_id;

typedef struct {
    t_ao_cloud_msg_type    msg_type;
    int is_online;        /* 0 - ofline, 1 - online */
} t_ao_conn_status;

typedef struct {
    t_ao_cloud_msg_type    msg_type;
} t_ao_video_start;

typedef struct {
    t_ao_cloud_msg_type    msg_type;
} t_ao_video_stop;

typedef struct {
    t_ao_cloud_msg_type    msg_type;
} t_ao_pzt;

typedef union {
    t_ao_cloud_msg_type command_type;
    t_ao_proxy_id       proxy_id;
    t_ao_conn_status    conn_status;
    t_ao_video_start    video_start;
    t_ao_video_stop     video_stop;
    t_ao_pzt            pzt;
} t_ao_cloud_msg;

/**********************************************************
 * Camera messages
 */
typedef enum {
    AO_CAM_UNDEF,           /* Can't understand the command */
    AO_CAM_RESULT           /* answer to command: OK or error */
} t_ao_cam_msg_type;

typedef struct {
    t_ao_cam_msg_type msg_type;
    int result;              /* 0 - ERROR, 1 - OK */
    char diagnostics[129];  /* '\0' or some zero-terminated string */
} t_ao_cam_result;

typedef union {
    t_ao_cam_msg_type msg_type;
    t_ao_cam_result result;
} t_ao_cam_msg;


#endif /* IPCAMTENVIS_AO_CMD_DATA_H */
