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
 Created by gsg on 17/10/17.
 This is an API to handle videostreaming outside - (re)start and stop
*/
#include "ao_cmd_data.h"

#ifndef IPCAMTENVIS_AT_CAM_VIDEO_H
#define IPCAMTENVIS_AT_CAM_VIDEO_H

/**************************************************
 * Start IPCam thread. Connection parameters will come later
 * @param params - camera start parameters
 * @return - 1 if OK, 0 if not
 */
int at_start_video_mgr();
/*****************************************************
 * Stop video translation
 * @param params - camera stop parameters
 * @return - 1 if OK, 0 if not
 */
int at_stop_video_mgr();
/***************************************************
 * Set stop flag to 1
 */
void at_set_stop_video_mgr();


#endif /* IPCAMTENVIS_AT_CAM_VIDEO_H */
