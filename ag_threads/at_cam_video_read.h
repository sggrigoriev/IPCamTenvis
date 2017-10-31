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
 Created by gsg on 30/10/17.
 Read vieo stream from camera. Reconnects until stop or success if looses connection
*/

#ifndef IPCAMTENVIS_AT_CAM_VIDEO_READ_H

#define IPCAMTENVIS_AT_CAM_VIDEO_READ_H

/***************************
 * Start getting cideo stream from the camera
 * @return - 1 is OK, 0 if not
 */
int at_start_video_read();
/*****************************
 * Stop read streaming (join)
 */
void at_stop_video_read();
/*****************************
 * Check if read stream runs
 * @return 1 if runs 0 if not
 */
int at_is_video_read_run();
/*******************************
 * Set sign to stop the thread
 */
void at_set_stop_video_read();

#endif /* IPCAMTENVIS_AT_CAM_VIDEO_READ_H */
