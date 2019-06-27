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
 Videoserver (WOWZA) RTSP protocol implementation
*/

#ifndef IPCAMTENVIS_AC_WOWZA_H
#define IPCAMTENVIS_AC_WOWZA_H

#include <stdlib.h>

#include "ac_cam_types.h"
/**
 * Get attribute value ("a=") from sdp with attr_name, given media type (video or audoi)
 * if media_type is NULL then the common attribute takes.
 *
 * @param sdp       - txt RTSP SDP presentation
 * @param attr_name - attribute name (a=<attr_name>)
 * @param media_type- NULL, video or audio
 * @return  - NULL if attr not found or attribute value as null-terminated string
 */
const char* ac_wowzaGetAttr(const char* sdp_ascii, const char* attr_name, const char* media_type);

/**
 * Initiaie session context
 *
 * @param sess              - session handler
 * @param wowza_session_id  - video server session ID
 * @return  - 0 if error, 1 if Ok
 */
int ac_WowzaInit(t_at_rtsp_session* sess, const char* wowza_session_id);

/**
 * Delete session context
 *
 * @param sess  - session handler
 */
void ac_WowzaDown(t_at_rtsp_session* sess);

/**
 * RTSP OPTIONS command implementation
 *
 * @param sess  - session handler
 * @return  - 0 if error, 1 if Ok
 */
int ac_WowzaOptions(t_at_rtsp_session* sess);

/**
 * RTSP ANNOUNCE command implementation
 *
 * @param sess          - session handler
 * @param description   - Cam description to be sent to Wowza
 * @return  - 0 if error, 1 if Ok
 */
int ac_WowzaAnnounce(t_at_rtsp_session* sess, const char* description);

/**
 * RTSP SETUP command implementation
 * @param sess          - session handler
 * @param media_type    - 0 video setup, 1 audio setup (AC_RTSP_VIDEO_SETUP, AC_RTSP_AUDIO_SETUP in ac_cam_types.h)
 * @return  - 0 if error, 1 if Ok
 */
int ac_WowzaSetup(t_at_rtsp_session* sess, int media_type);

/**
 * RTSP PLAY command implementation
 * @param sess  - session handler
 * @return  - 0 if error, 1 if Ok
 */
int ac_WowzaPlay(t_at_rtsp_session* sess);

/**
 * RTSP PLAY command implementation
 * @param sess  - session handler
 * @return  - 0 if error, 1 if Ok
 */
int ac_WowzaTeardown(t_at_rtsp_session* sess);

/* <vs_url>:<port>/ppcvideoserver/<vs_session_id> */
/**
 * Create URL to access WOWZA server by following format:
 * <vs_url>:<port>/ppcvideoserver/<vs_session_id>
 *
 * @param url           - buffer for URL made
 * @param size          - buffer size
 * @param protocol      - protocol name - RTSP could be changed to internal constant
 * @param vs_url        - video server (Wowza) URL
 * @param port          - port number
 * @param vs_session_id - Wowza session ID
 * @return  - pointer to the buffer
 */
const char* ac_make_wowza_url(char *url, size_t size, const char* protocol, const char* vs_url, int port, const char* vs_session_id);
/*  - use same connection as for RTSP negotiations */
/**
 * Get the connected socket for interleaved mode.
 * Streaming uses same connection as for RTSP handshake
 *
 * @param sess  - session handler
 * @return  - 0 if error, 1 if Ok
 */
int getWowzaConnSocket(t_at_rtsp_session* sess);

#endif /* IPCAMTENVIS_AC_WOWZA_H */
