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
 Created by gsg on 18/10/18.
 Camera's generated files sender (SF).
*/

#ifndef IPCAMTENVIS_AT_CAM_FILES_SENDER_H
#define IPCAMTENVIS_AT_CAM_FILES_SENDER_H

/**
 * Start SF thread
 *
 * @param rd_sock   - open TCP read socket
 * @return  - 1
 */
int at_start_sf(int rd_sock);

/**
 * Sync stop SF thread (join() inside
 */
void at_stop_sf();

/**
 * Set async stop for SF externally
 */
void at_set_stop_sf();

#endif /* IPCAMTENVIS_AT_CAM_FILES_SENDER_H */
