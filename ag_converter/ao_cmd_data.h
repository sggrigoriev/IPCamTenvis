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

/*************************************************************************
 * All from and to Agent messages internak format: from Proxy, from Cam
 */

typedef enum {
    AO_UNDEF,                   /* Can't understand the command */
    AO_CLOUD_PROXY_ID,          /* Proxy device ID */
    AO_CLOUD_CONNECTION_STATE,  /* Off line or on line */
    AO_CLOUD_VIDEO_PARAMS,      /* Video-server connection parameters */
    AO_COUD_START_VIDEO,        /* First time - open connection, second time - Start video streaming */
    AO_CLOUD_STOP_VIDEO,        /* Stop video streaming */
    AO_CLOUD_PZT,               /* PZT command */
/* CAMERA PART */
    AO_CAM_VIDEO_CONNECTON_RESULT,         /* 1 and sessionID or 0 and diagnostics */
    AO_CAM_VIDEO_PLAY_START_RESULT,        /* 1 as OK, 0 and diagnostics */
    AO_CAM_VIDEO_PLAY_STOP_RESULT,          /* 1 as OK, 0 and diagnostics */

/* Own messages */
    AO_OWN_ERROR
} t_ao_msg_type;

/* CLOUD PART */
typedef struct{
    t_ao_msg_type    msg_type;
    char proxy_device_id[LIB_HTTP_DEVICE_ID_SIZE];
} t_ao_proxy_id;

typedef struct {
    t_ao_msg_type    msg_type;
    int is_online;        /* 0 - ofline, 1 - online */
} t_ao_conn_status;

typedef struct {
    t_ao_msgtype msg_type;
    int result_code;
    bool apiServerSsl;
    char videoServer[LIB_HTTP_MAX_URL_SIZE];
    bool videoServerSsl;
    char sessionId[LIB_HTTP_AUTHENTICATION_STRING_SIZE];
} t_ao_video_conn_data;

typedef struct {
    t_ao_msg_type    msg_type;
} t_ao_video_start;

typedef struct {
    t_ao_msg_type    msg_type;
} t_ao_video_stop;

typedef struct {
    t_ao_msg_type    msg_type;
} t_ao_pzt;

/* Camera part */
typedef struct {
    t_ao_cam_msg_type msg_type;
    int result;              /* 0 - ERROR, 1 - OK */
    char session_id[AC_CAM_RSTP_SESSION_ID_LEN];  /* '\0' or some zero-terminated string */
    char diagnostics[129];  /* '\0' or some zero-terminated string */
} t_ao_cam_video_connection_result;

typedef struct {
    t_ao_cam_msg_type msg_type;
    int result;              /* 0 - ERROR, 1 - OK */
    char diagnostics[129];  /* '\0' or some zero-terminated string */
} t_ao_cam_video_start_result;

typedef struct {
    t_ao_cam_msg_type msg_type;
    int result;              /* 0 - ERROR, 1 - OK */
    char diagnostics[129];  /* '\0' or some zero-terminated string */
} t_ao_cam_video_stop_result;

/* Own part - Agent's messages */
typedef struct {
    t_ao_msg_type msg_type;
    int rc;
    char error[129];
} t_ao_own_error;

typedef union {
    t_ao_msg_type command_type;
    t_ao_proxy_id       proxy_id;
    t_ao_conn_status    conn_status;
    t_ao_video_conn_data video_conn_data;
    t_ao_video_start    video_start;
    t_ao_video_stop     video_stop;
/* CAM answers */
    t_ao_cam_video_connection_result    cam_video_connection_result;
    t_ao_cam_video_start_result         cam_video_start_result;
    t_ao_cam_video_stop_result          cam_video_stop_result;

    t_ao_pzt            pzt;
    t_ao_own_error      own_error;
} t_ao_msg;

#endif /* IPCAMTENVIS_AO_CMD_DATA_H */
