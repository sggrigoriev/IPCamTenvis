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

#ifndef IPCAMTENVIS_AC_RTSP_H
#define IPCAMTENVIS_AC_RTSP_H

#include "ao_cma_cam.h"
#include "ao_cmd_data.h"

typedef enum {
    AC_CAMERA,
    AC_WOWZA
} t_ac_rtsp_device;

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

int ac_open_session(t_ac_rtsp_device device_type, const char* url);
void ac_close_session(t_ac_rtsp_device device_type);
int ac_req_options(t_ac_rtsp_device device_type, char* head, size_t h_size, char* body, size_t b_size);
int ac_req_setup(t_ac_rtsp_device device_type, char* head, size_t h_size, char* body, size_t b_size, int cient_port);
int ac_req_play(t_ac_rtsp_device device_type, char* head, size_t h_size, char* body, size_t b_size);
int ac_req_teardown(t_ac_rtsp_device device_type, char* head, size_t h_size, char* body, size_t b_size);

int ac_get_server_port(const char* msg);


int ac_req_cam_describe(char* head, size_t h_size, char* body, size_t b_size);

/* [login:password@]ip:port/resolution/ */
const char* ac_makeCamURL(char *url, size_t size, const char* ip, int port, const char* login, const char* pwd, t_ao_cam_res resolution);

/******************************************** Video Server part *******************************************************/

int ac_req_vs_announce1(char* cam_describe_body, char* head, size_t h_size, char* body, size_t b_size);
int ac_req_vs_announce2(char* head, size_t h_size, char* body, size_t b_size);
/* <vs_url>:<port>/ppcvideoserver/<vs_session_id> */
const char* ac_makeVSURL(char *url, size_t size, const char* vs_url, int port, const char* vs_session_id);


#endif /* IPCAMTENVIS_AC_RTSP_H */
