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
*/

#ifndef IPCAMTENVIS_AC_VIDEO_H
#define IPCAMTENVIS_AC_VIDEO_H

/*
 * 1. Get streaming & WS connection parameters
 * 2. Make initial Cam setuo
 * 3. Run WebSocket interface
 * 4. Run Cam's async interface (phase - II
 */
int ac_connect_video();

void ac_disconnect_video();

/*
 * Rus video streaming (and audio - later)
 */
int ac_start_video();

/*
 * Stops videostreaming
 */

void ac_stop_video();

/*
 * Return 1 if treaming threads work
 */
int ac_streaming_run();


void ac_send_stream_initiation();
void ac_send_stream_confirmation();

#endif /* IPCAMTENVIS_AC_VIDEO_H */
