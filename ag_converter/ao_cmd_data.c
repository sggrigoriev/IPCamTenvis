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
 Created by gsg on 22/09/18.
*/
#include "ao_cmd_data.h"

static const char* CAM_EVENTS_NAMES[AC_CAM_EVENTS_SIZE] = {
    "AC_CAM_EVENT_UNDEF",
    "AC_CAM_START_MD", "AC_CAM_STOP_MD", "AC_CAM_START_SD", "AC_CAM_STOP_SD", "AC_CAM_START_IO", "AC_CAM_STOP_IO",
    "AC_CAM_STOP_SERVICE"
};

const char* ac_cam_evens2string(t_ac_cam_events e) {
    return ((e < AC_CAM_EVENT_UNDEF) || (e >= AC_CAM_EVENTS_SIZE))?
        "Unknown event" :
        CAM_EVENTS_NAMES[e];
}
