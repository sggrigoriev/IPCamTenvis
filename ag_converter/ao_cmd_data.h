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
    AO_UNDEF,                       /* Can't understand the command */
/*------ To Agent from Cloud/Proxy */
    AO_IN_PROXY_ID,              /* Proxy device ID */
    AO_IN_CONNECTION_STATE,      /* Off line or on line */

    AO_IN_VIDEO_PARAMS,          /* Video-server connection parameters */
    AO_IN_START_STREAM_0,         /* Command from cloud to start connection with Video server */
    AO_IN_STREAM_SESS_DETAILS,   /* Answer from the cloud with stream session details */
    AO_IN_SS_TO_1_RESP,          /* Answer from cloud - they reflected the sccesful connection from Cam */
    AO_IN_SS_TO_0_RESP,          /* Answer from cloud - they reflected Cam vidoe disconnection from Wowza */

    AO_IN_PZT,                   /* PZT command */
/* ---- To Cloud from Agent */
    AO_OUT_STREAM_SESSION_REQ,          /* VM requested Stream session details (stream_id) from the cloud */
    AO_CAM_CONNECTED,                   /* VC repors about connection with video server */
    AO_OUT_SS_1_REQUEST,                /* VM requested to confirm cloud its connection with Wowza */
    AO_CAM_DISCONNECTED,                /* VC reports the cam is disconnected from the video server */
    AO_OUT_SS_0_REQUEST,                /* VM requested to confirm cloud its disconnection with Wowza */
} t_ao_msg_type;

/* AO_IN_PROXY_ID */
typedef struct{
    t_ao_msg_type    msg_type;
    char proxy_device_id[LIB_HTTP_DEVICE_ID_SIZE];
} t_ao_in_proxy_id;

/* AO_IN_CONNECTION_STATE */
typedef struct {
    t_ao_msg_type    msg_type;
    int is_online;        /* 0 - ofline, 1 - online */
} t_ao_in_connection_state;

/* AO_IN_VIDEO_PARAMS */
typedef struct {
    t_ao_msgtype msg_type;
    int result_code;
    bool apiServerSsl;
    char videoServer[LIB_HTTP_MAX_URL_SIZE];
    bool videoServerSsl;
    char sessionId[LIB_HTTP_AUTHENTICATION_STRING_SIZE];
} t_ao_in_video_params;

/* AO_IN_START_STREAM_0 */
typedef struct {
    t_ao_msg_type    msg_type;
} t_ao_in_start_stream_0;

/* AO_IN_STREAM_SESS_DETAILS */
typedef struct {
    t_ao_msg_type    msg_type;
    char session_id[AC_CAM_RSTP_SESSION_ID_LEN];
} t_ao_in_stream_sess_details;

/* AO_IN_SS_TO_1_RESP */
typedef struct {
    t_ao_msg_type    msg_type;
} t_ao_in_ss_to_1_resp;

/* AO_IN_SS_TO_0_RESP */
typedef struct {
    t_ao_msg_type    msg_type;
} t_ao_in_ss_to_0_resp;

/* AO_IN_PZT */
typedef struct {
    t_ao_msg_type    msg_type;
} t_ao_in_pzt;

/* AO_OUT_STREAM_SESSION_REQ */
typedef struct {
    t_ao_cam_msg_type msg_type;
} t_ao_out_stream_session_req;

/* AO_CAM_CONNECTED */
typedef struct {
    t_ao_cam_msg_type msg_type;
    int connected;              /* 1 if connected, 0 if failed */
} t_ao_cam_connected;

/* AO_OUT_SS_1_REQUEST */
typedef struct {
    t_ao_cam_msg_type msg_type;
    int connected;
} t_ao_out_ss_1_request;

/* AO_CAM_DISCONNECTED */
typedef struct {
    t_ao_cam_msg_type msg_type;
    int error_disconnection;
} t_ao_cam_disconnected;
/* AO_OUT_SS_0_REQUEST */
typedef struct {
    t_ao_cam_msg_type msg_type;
} t_ao_out_ss_0_request;

typedef union {
    t_ao_msg_type           command_type;
    t_ao_in_proxy_id            in_proxy_id;
    t_ao_in_connection_state    in_connection_state;

    t_ao_in_video_params        in_video_params;
    t_ao_in_start_stream_0      in_start_stream_0;
    t_ao_in_stream_sess_details in_stream_sess_details;
    t_ao_in_ss_to_1_resp        in_ss_to_1_resp;
    t_ao_in_ss_to_0_resp        in_ss_to_0_resp;

    t_ao_in_pzt                 in_pzt;

    t_ao_out_stream_session_req out_stream_session_req;

    t_ao_cam_connected          cam_connected;
    t_ao_out_ss_1_request       out_ss_1_request;
    t_ao_cam_disconnected       cam_disconnected;
    t_ao_out_ss_0_request       out_ss_0_request;
} t_ao_msg;

#endif /* IPCAMTENVIS_AO_CMD_DATA_H */
