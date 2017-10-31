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
 Read the video flow from the camera
*/

#include "at_cam_video_read.h"

/********************************************
 * Local data
 */

static volatile int stop = 0;
static volatile int stopped = 1;

/*********************************************
 * Global functions
 */

int at_start_video_read() {
    return 0;
}

void at_stop_video_read() {
    /**************************/

    stopped = 1;
}

int at_is_video_read_run() {
    return !stopped;
}

void at_set_stop_video_read() {
    stop = 1;
}

