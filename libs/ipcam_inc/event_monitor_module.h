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
 * This is a non-reetneran C wrapper under C++ event monitor
 * It has 3 functions:
 * int em_init(const char* ip) - to initialize the monitor
 * int em_function()           - function returning the Cam's events
 * void em_deinit()            - close the monitor
 Created by gsg on 21/09/18.
*/

#ifndef LIBS_EVENT_MONITOR_MODULE_H
#define LIBS_EVENT_MONITOR_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#define EMM_IO_EVENT    -1   /* For connector on the back of the Cam. */
#define EMM_MD_EVENT    -2
#define EMM_AB_EVENT    -3   /* Abnormal event. Smth wrong in Danish Kingdom */
#define EMM_SD_EVENT    -4

#define EMM_READ_ERROR  -5
#define EMM_NO_RESPOND  -6
#define EMM_TIMEOUT     0
#define EMM_SELECT_ERR  -7
#define EMM_ALRM_IGNOR  -8



int em_init(const char* ip);
void em_deinit();
int em_function(int to_sec);

#ifdef __cplusplus
}
#endif
#endif /* LIBS_EVENT_MONITOR_MODULE_H */
