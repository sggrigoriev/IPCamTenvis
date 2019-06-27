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

#include <stdint.h>
#include "ac_cam_types.h"

/**
 * Initiate the session context
 * @param device        - server type (see ac_cam_types.h)
 * @param ip            - IP address as a string
 * @param port          - port number
 * @param session_id    - session ID (camera or Wowza depending on device)
 * @return  - session handler or NULL if error
 */
t_at_rtsp_session* ac_rtsp_init(t_ac_rtsp_device device, const char* ip, int port, const char* session_id);

/**
 * Delete session context
 * @param sess  - session handler
 */
void ac_rtsp_deinit(t_at_rtsp_session* sess);

/**
 * RTSP OPTIONS command implementation
 * @param sess  - session handler
 * @return  - 0 if error, 1 if Ok
 */
int ac_req_options(t_at_rtsp_session* sess);

/**
 * RTSP DESCRIBE command implementation
 * @param sess              - session handler
 * @param dev_description   - buffer with device description (Wowza or Camera)
 * @param size              - buffer size
 * @return  - 0 if error, 1 if Ok
 */
int ac_req_cam_describe(t_at_rtsp_session* sess, char* dev_description, size_t size);

/**
 * RTSP ANNOUNCE command implementation
 * @param sess              - session handler
 * @param dev_description   - device description (Wowza or Camera)
 * @return  - 0 if error, 1 if Ok
 */
int ac_req_vs_announce(t_at_rtsp_session* sess, const char* dev_description);

/**
 * RTSP SETUP command implementation
 * @param sess      - session handler
 * @param is_video  - 1 video enabled, 0 video disabled (only for debug)
 * @param is_audio  - 1 audio enabled, 0 video disabled
 * @return  - 0 if error, 1 if Ok
 */
int ac_req_setup(t_at_rtsp_session* sess, int is_video, int is_audio);

/**
 * RTSP PLAY command implementation
 * @param sess  - session handler
 * @return  - 0 if error, 1 if Ok
 */
int ac_req_play(t_at_rtsp_session* sess);

/**
 * RTSP TEARDOWN command implementation
 * @param sess - session handler
 * @return  - 0 if error, 1 if Ok
 */
int ac_req_teardown(t_at_rtsp_session* sess);

/**
 * Initiate interleave streaming connection (UDP is not used)
 * @param sess_in   - input streaming session handler (Camera)
 * @param sess_out  - output streaming session handler (Wowza)
 * @return  - 0 if error, 1 if Ok
 */
int ac_rtsp_open_streaming_connecion(t_at_rtsp_session* sess_in, t_at_rtsp_session* sess_out);

/**
 * Close streaming connection
 */
void ac_rtsp_close_streaming_connecion();

/**
 * Start streaming (Cam->Agent->Wowza)
 * @return  - 0 if error, 1 if Ok
 */
int ac_rtsp_start_streaming();

/**
 * Stop streaming
 */
void ac_rtsp_stop_streaming();

#endif /* IPCAMTENVIS_AC_RTSP_H */
