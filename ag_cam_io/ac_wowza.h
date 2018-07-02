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
/***************************************************
 * Get attribute value ("a=") from sdp with attr_name, given media type (video or audoi)
 * if media_type is NULL then the common attribute takes.
 * @param sdp           txt RTSP SDP presentation
 * @param attr_name     attribute name (a=<attr_name>)
 * @param media_type    NULL, video or audio
 * @return              NULL if attr not found or attribute value as null-terminated string
 */
const char* ac_wowzaGetAttr(const char* sdp_ascii, const char* attr_name, const char* media_type);

int ac_WowzaInit(t_at_rtsp_session* sess, const char* wowza_session_id);
void ac_WowzaDown(t_at_rtsp_session* sess);

int ac_WowzaOptions(t_at_rtsp_session* sess);
int ac_WowzaAnnounce(t_at_rtsp_session* sess, const char* description);
int ac_WowzaSetup(t_at_rtsp_session* sess, int media_type);
int ac_WowzaPlay(t_at_rtsp_session* sess);
int ac_WowzaTeardown(t_at_rtsp_session* sess);

/* <vs_url>:<port>/ppcvideoserver/<vs_session_id> */
const char* ac_make_wowza_url(char *url, size_t size, const char* protocol, const char* vs_url, int port, const char* vs_session_id);
/* Get the connected socket for interleaved mode - use same connection as for RTSP negotiations */
int getWowzaConnSocket(t_at_rtsp_session* sess);

#endif /* IPCAMTENVIS_AC_WOWZA_H */
