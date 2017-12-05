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
    AO_RES_UNDEF,
    AO_RES_LO,
    AO_RES_HI
} t_ao_cam_res;

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
    char uri[LIB_HTTP_MAX_URL_SIZE];
    int number;
    t_ac_rtsp_body b;
}t_ac_rtsp_msg;

const char* ao_makeURI(char *uri, size_t size, const char* ip, int port, const char* login, const char* pwd, t_ao_cam_res resolution);

t_ac_rtsp_type ao_get_msg_type(const char* msg);
int ao_get_msg_number(const char* msg);
void ao_get_uri(char* uri, size_t size, const char* msg);
void ao_cam_replace_uri(char* msg, size_t size, const char* new_uri);
int ao_get_client_port(const char* msg);
int ao_get_server_port(const char* msg);

int ao_cam_encode(t_ao_msg data, const char* to_cam_msg, size_t size);


#endif /* IPCAMTENVIS_AO_CMA_CAM_H */
