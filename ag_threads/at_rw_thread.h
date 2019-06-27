/*
 *  Copyright 2018 People Power Company
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
 Created by gsg on 17/03/18.
 Streaming thread(s)
 Initially was designed for interleaved and non-interleaved modes
 Finally just interleaved mode is supported. Has to be cleaned-up.
*/

#ifndef IPCAMTENVIS_AT_RW_THREAD_H
#define IPCAMTENVIS_AT_RW_THREAD_H

#include "ac_cam_types.h"

/**
 * Set parameters for interleaved streaming type
 *
 * @param rd        - read socket (stream from camera)
 * @param wr        - write socket (stream  to Wowza)
 * @param cam_sess  - camera RTSP session context
 * @return  - 0 if error, 1 if Ok
 */
int at_set_interleaved_rw(int rd, int wr, t_at_rtsp_session* cam_sess);

/**
 * Start getting video stream from the camera in RT mode - using 4 streams: video & audio
 *
 * @return - 1 is OK, 0 if not
 */
int at_start_rw();

/**
 * Stop read streaming (join)
 */
void at_stop_rw();

/**
 * Check if read stream runs
 *
 * @return  - 1 if runs 0 if not
 */
int at_is_rw_thread_run();

#endif /* IPCAMTENVIS_AT_RW_THREAD_H */
