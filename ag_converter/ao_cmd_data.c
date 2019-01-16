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
#include <string.h>
#include <stdio.h>

#include "ao_cmd_data.h"

static const char* CAM_EVENTS_NAMES[AC_CAM_EVENTS_SIZE] = {
        "AC_CAM_EVENT_UNDEF",
        "AC_CAM_START_MD", "AC_CAM_STOP_MD", "AC_CAM_START_SD", "AC_CAM_STOP_SD", "AC_CAM_START_IO", "AC_CAM_STOP_IO",
        "AC_CAM_MADE_SNAPSHOT", "AC_CAM_RECORD_VIDEO",
        "AC_CAM_STOP_SERVICE", "AC_CAM_TIME_TO_PING", "AC_CAM_GOT_FILE"
};

t_ac_cam_events ac_cam_string2event(const char* string) {
    t_ac_cam_events i;
    if(!string) return AC_CAM_EVENT_UNDEF;
    for(i = AC_CAM_EVENT_UNDEF; i < AC_CAM_EVENTS_SIZE; i++) {
        if(!strcmp(string, CAM_EVENTS_NAMES[i])) return i;
    }
    return AC_CAM_EVENT_UNDEF;
}
const char* ac_cam_event2string(t_ac_cam_events event) {
    if((event < 0) || (event >= AC_CAM_EVENTS_SIZE)) event = AC_CAM_EVENT_UNDEF;
    return CAM_EVENTS_NAMES[event];
}
/*
 * Alert EM->Agent (got file) and/or Agent->SF (got file, got stapshot)
 */
const char* ao_make_got_files(const char* path, char* buf, size_t size) {
    const char* alert = "{\""AC_ALERT_NAME"\": \"%s\", \""AC_ALERT_MSG"\": \"%s\"}";
    snprintf(buf, size-1, alert, ac_cam_event2string(AC_CAM_GOT_FILE), path);
    return buf;
}
/*
 * Alert EM-> Agent about motion od sound detection
 */
const char* ao_make_MDSD(t_ac_cam_events event, time_t start_date, time_t end_date, char* buf, size_t size) {
    const char* alert = "{\""AC_ALERT_NAME"\" : \"%s\"}";
    const char* alert_start = "{\""AC_ALERT_NAME"\" : \"%s\", \""AC_ALERT_START"\" : %lu}";
    const char* alert_stop = "{\""AC_ALERT_NAME"\" : \"%s\", \""AC_ALERT_START"\" : %lu, \""AC_ALERT_END"\" : %lu}";

    if(!start_date)
        snprintf(buf, size-1, alert, ac_cam_event2string(event));
    else if(!end_date)
        snprintf(buf, size-1, alert_start, ac_cam_event2string(event), start_date);
    else
        snprintf(buf, size-1, alert_stop, ac_cam_event2string(event), start_date, end_date);

    buf[size-1] = '\0';
    return buf;

}
