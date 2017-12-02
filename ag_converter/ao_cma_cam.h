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

#include "ao_cmd_data.h"

typedef enum {
    AC_UNDEFINED,
    AC_DESCRIBE,
    AC_ANNOUNCE,
    AC_SETUP,
    AC_PLAY,
    AC_TEARDOWN,
} t_ac_rtsp_type;

typedef struct {
    int video_tracks_number;
} t_ac_rtsp_describe;
typedef struct {
    int video_track_number;
} t_ac_rtsp_announce;
typedef struct {
    int track_number;
    int client_port;
    int server_port;
} t_ac_rtsp_setup;
typedef struct {
    char session_id[DEFAULT_CAM_RSTP_SESSION_ID_LEN];
} t_ac_rtsp_play;

typedef union {
    t_ac_rtsp_describe describe;
    t_ac_rtsp_announce announce;
    t_ac_rtsp_setup setup;
    t_ac_rtsp_play play;
} t_ac_rtsp_body;

typedef struct {
    t_ac_rtsp_type msg_type;
    char ip_port[66];
    int number;
    t_ac_rtsp_body b;
}t_ac_rtsp_msg;

t_ac_rtsp_msg ao_cam_decode_req(const char* cam_message);
t_ac_rtsp_msg ao_cam_decode_ans(t_ac_rtsp_type req_type, int req_number, const char* cam_message);
const char* ao_cam_replace_addr(char* msg, size_t size, const char* ip_port);
const char* ao_makeIPPort(char* buf, size_t size, const char* ip, int port);


#endif /* IPCAMTENVIS_AO_CMA_CAM_H */
