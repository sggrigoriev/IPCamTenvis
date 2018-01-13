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
 Created by gsg on 10/01/18.
*/

#ifndef IPCAMTENVIS_AC_WOWZA_H
#define IPCAMTENVIS_AC_WOWZA_H

#include <stdlib.h>

#include "ac_cam_types.h"

t_at_rtsp_session* ac_WowzaInit();
void ac_WowzaDown(t_at_rtsp_session* sess);

int ac_WowzaOpenSession(t_at_rtsp_session* sess, const char* wowza_session);
void ac_WowzaCloseSession(t_at_rtsp_session* sess);

int ac_WowzaOptions(t_at_rtsp_session* sess);
int ac_WowzaAnnounce(t_at_rtsp_session* sess, const char* description);
int ac_WowzaSetup(t_at_rtsp_session* sess, int client_port);
int ac_WowzaPlay(t_at_rtsp_session* sess);
int ac_WowzaTeardown(t_at_rtsp_session* sess);

/* <vs_url>:<port>/ppcvideoserver/<vs_session_id> */
const char* ac_make_wowza_url(char *url, size_t size, const char* vs_url, int port, const char* vs_session_id);

#endif /* IPCAMTENVIS_AC_WOWZA_H */
