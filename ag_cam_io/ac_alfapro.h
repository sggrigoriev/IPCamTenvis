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
 RTSP implementation for AlfaPro camera on cURL.
 cURL has to be changed to gstreamer lib.
*/

#ifndef IPCAMTENVIS_AC_ALFAPRO_H
#define IPCAMTENVIS_AC_ALFAPRO_H

#include <stdlib.h>

#include "ac_cam_types.h"
/**
 * RTSP session context initiation
 * @param sess -    session context
 * @return  - 0 if error, 1 if OK
 */
int ac_alfaProInit(t_at_rtsp_session* sess);

/**
 * RTSP session context deletion
 * @param sess -    session context
 */
void ac_alfaProDown(t_at_rtsp_session* sess);

/**
 * RTSP OPTIONS command implementation
 * @param sess -            session context
 * @param suppress_info -   1 - suppress operation result logging
 * @return  - 0 if error, 1 if success
 */
int ac_alfaProOptions(t_at_rtsp_session* sess, int suppress_info);

/**
 * RTSP DESCRIBE command implementation
 * @param sess -    Session context
 * @param descr -   Buffer for answer with description
 * @param size -    Buffer size
 * @return  - 0 if error 1 if Ok
 */
int ac_alfaProDescribe(t_at_rtsp_session* sess, char* descr, size_t size);

/**
 * RTSP SETUP command implementation
 * @param sess -        Session context
 * @param media_type -  AC_RTSP_VIDEO_SETUP or AC_RTSP_AUDIO_SETUP (see ac_cam_types.h)
 * @return  - 0 if error, 1 if Ok
 */
int ac_alfaProSetup(t_at_rtsp_session* sess, int media_type);

/**
 * RTSP PLAY command implementation
 * @param sess -    session context
 * @return  - 0 if error, 1 if Ok
 */
int ac_alfaProPlay(t_at_rtsp_session* sess);

/**
 * RTSP TEARDOWN command implementation
 * @param sess -    session context
 * @return  - 0 if error, 1 if Ok
 */
int ac_alfaProTeardown(t_at_rtsp_session* sess);

/**
 * To get the socket opened by cURL for RTSP session
 * @param sess -    session context
 * @return  - -1 if error, connected socket if Ok
 */
int getAlfaProConnSocket(t_at_rtsp_session* sess);

#endif /* IPCAMTENVIS_AC_ALFAPRO_H */
