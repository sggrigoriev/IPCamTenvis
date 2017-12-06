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
 Created by gsg on 13/11/17.
 Video server <-> proxy <-> camera RSTP portocol support
 The Agent plays as proxy for cloud viewer and camera
 Contains RTSP protocol support
*/

#include "ao_cma_cam.h"
#include "ao_cmd_data.h"

#ifndef IPCAMTENVIS_AC_RTSP_H
#define IPCAMTENVIS_AC_RTSP_H


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

int ac_rtsp_init();
void ac_rtsp_down();

int ac_open_cam_session(const char* cam_uri);
void ac_close_cam_session();

int ac_req_cam_options(char* head, size_t h_size, char* body, size_t b_size);
int ac_req_cam_setup(char* head, size_t h_size, char* body, size_t b_size, int cient_port);
int ac_req_cam_play(char* head, size_t h_size, char* body, size_t b_size);

/* [login:password@]ip:port/resolution/ */
const char* ao_makeCamURI(char *uri, size_t size, const char* ip, int port, const char* login, const char* pwd, t_ao_cam_res resolution);

#endif /* IPCAMTENVIS_AC_RTSP_H */
