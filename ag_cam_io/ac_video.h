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
 Created by gsg on 25/02/18.
 High-level interfaces to manage video streaming
*/

#ifndef IPCAMTENVIS_AC_VIDEO_H
#define IPCAMTENVIS_AC_VIDEO_H

#include <unistd.h>

/**
 * Prepare streaming:
 * 1. Get streaming & WS connection parameters
 * 2. Make initial Cam setuo
 * 3. Run WebSocket interface
 * 4. Run Cam's async interface (phase - II)
 *
 * @return  - 0 if error, 1 if Ok
 */
int ac_connect_video();

/**
 * Close streaming session
 */
void ac_disconnect_video();

/**
 * Run video streaming
 *
 * @param is_video  - 0 no video (debugging only!), 1 video enabled
 * @param is audio  - 0 no audio, 1 audio enabled
 * @return  - 0 if error, 1 if Ok
 */
int ac_start_video(int is_video, int is_audio);

/**
 * Stops video streaming
*/
void ac_stop_video();

/*
 * Thread-protected functions
 */

/**
 * Set module static parameter to be used in anther thread
 * @param err   - string with error text
 */
void ac_set_stream_error(const char* err);

/**
 * Clear error message
 */
void ac_clear_stream_error();

/**
 * Copy error string to the local memory
 *
 * @param buf   - buffer to store the string
 * @param size  - buffer size
 * @return  - pointer to the buffer
 */
const char* ac_get_stream_error(char* buf, size_t size);

/**
 * Get the presence of error message
 *
 * @return  - 0 error string is empty, 1 error string is not empty
 */
int ac_is_stream_error();

/**
 * Get video server (Wowza) session ID
 * @param buf   - buffer to save the session ID
 * @param size  - buffer size
 * @return  - pointer to the buffer
 */
const char* ac_get_session_id(char* buf, size_t size);

#endif /* IPCAMTENVIS_AC_VIDEO_H */
