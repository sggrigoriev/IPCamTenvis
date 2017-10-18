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

#ifndef IPCAMTENVIS_AT_PROXY_RW_H
#define IPCAMTENVIS_AT_PROXY_RW_H

/* Start the thread. Return 1 */
int at_start_proxy_rw();

/* Stop the thread */
void at_stop_proxy_rw();

/* Say stop to the thread - external interface for async thread stop */
void at_set_stop_proxy_rw();

/* Say stop to the r/w threads - external interface for async threads stop */
void at_set_stop_proxy_rw_children();

/* Check the stop flag. Return 1 if flag is on, 0 if not*/
int at_are_childs_stop();

#endif /* IPCAMTENVIS_AT_PROXY_RW_H */
