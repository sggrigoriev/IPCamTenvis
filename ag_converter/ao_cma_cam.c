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
*/

#include "ao_cma_cam.h"

/*
 * Notify Agent about the event
 * if start_date or end_date is 0 fields are ignored
 * {"alertName" : "AC_CAM_STOP_MD", "startDate" : 1537627300, "endDate" : 1537627488}
 */
const char* ao_make_cam_alert(t_ac_cam_events event, time_t start_date, time_t end_date, char* buf, size_t size) {
    const char* alert = "{\"alertName\" : \"%s\"}";
    const char* alert_start = "{\"alertName\" : \"%s\", \"startDate\" : %lu}";
    const char* alert_stop = "{\"alertName\" : \"%s\", \"startDate\" : %lu, \"endDate\" : %lu}";

    if(!start_date)
        snprintf(buf, size-1, alert, ac_cam_evens2string(event));
    else if(!end_date)
        snprintf(buf, size-1, alert_start, ac_cam_evens2string(event), start_date);
    else
        snprintf(buf, size-1, alert_stop, ac_cam_evens2string(event), start_date, end_date);

    buf[size] = '\0';
    return buf;
}

