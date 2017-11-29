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
*/

#include "ao_cmd_data.h"

#ifndef IPCAMTENVIS_AC_RTSP_H
#define IPCAMTENVIS_AC_RTSP_H

#define AC_CAM_RSTP_SESSION_ID_LEN  20

typedef enum {
    AC_UNDEFINED,
    AC_DESCRIBE,
    AC_ANNOUNCE,
    AC_SETUP,
    AC_PLAY,
    AC_TEARDOWN,
    AC_JUST_ANSWER
} t_ac_rtsp_type;

typedef struct {
    t_ac_rtsp_type msg_type;
    int video_track_number;
} t_ac_rtsp_describe;
typedef struct {
    t_ac_rtsp_type msg_type;
    int video_track_number;
} t_ac_rtsp_announce;
typedef struct {
    t_ac_rtsp_type msg_type;
    int track_number;
    int client_port;
    int server_port;
} t_ac_rtsp_setup;
typedef struct {
    t_ac_rtsp_type msg_type;
    char session_id[AC_CAM_RSTP_SESSION_ID_LEN];
} t_ac_rtsp_play;
typedef struct {
    t_ac_rtsp_type msg_type;
} t_ac_rtsp_teardown;
typedef union {
    t_ac_rtsp_type msg_type;
    int answer;        /* 0 - request, 1 - answer */
    int status;         /* 200 - OK, others - error code */
    t_ac_rtsp_describe describe;
    t_ac_rtsp_announce announce;
    t_ac_rtsp_setup setup;
    t_ac_rtsp_play play;
    t_ac_rtsp_teardown reardown;
} t_ac_rtsp_msg;

t_ac_rtsp_msg rtsp_parse(const char* rtsp_msg);

/*********************************************
 * Make internet connection
 * @param connection_params -  parameters to make connection
 * @return  connected socket or 01 if error
 */
int ac_rtsp_connect(t_ao_in_video_params connection_params);

/**********************************************
 * Disconnects from VS and from camera
 */
void ac_rtsp_disconnect();


#endif /* IPCAMTENVIS_AC_RSTP_H */
