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
*/

#include "pu_logger.h"

#include "ac_video_interface.h"
#include "at_cam_video_read.h"
#include "at_cam_video_write.h"

#include "at_cam_video.h"

int at_cam_video_start(t_ao_video_start params) {
    if(at_is_video_run()) at_cam_video_stop();
    at_start_video_write(params);
    at_start_video_read(params);

    return 1;
}

int at_cam_video_stop() {
    at_set_stop_video_write();
    at_set_stop_video_read();
    at_stop_video_read();
    at_stop_video_write();
    ac_close_connections();
    return 1;
}

int at_is_video_run() {
    return (at_is_video_read_run() && at_is_video_write_run());
}

