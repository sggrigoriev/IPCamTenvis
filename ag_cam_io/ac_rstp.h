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
 Created by gsg on 13/11/17.
 Video server <-> proxy <-> camera RSTP portocol support
 The Agent plays as proxy for cloud viewer and camera
*/

#include "ao_cmd_data.h"

#ifndef IPCAMTENVIS_AC_RSTP_H
#define IPCAMTENVIS_AC_RSTP_H

/*******************************************************************
 * Make VS-Agent and Agent-Camera connection
 * @param responce - operation result
 * @param size - max buffer size
 * @return - 1 if OK, 0 if not
 */
int ac_connect(char* responce, size_t size);
/**********************************************
 * Disconnects from VS and from camera
 * @param responce - operation result
 * @param size - max buffer size
 */
void ac_disconnect(char* responce, size_t size);
/*********************************************************************
 * Start video playing
 * @param responce - operation result
 * @param size - max buffer size
 * @return - 1 if OK, 0 if not
 */
int ac_start_play(char* responce, size_t size);
/*********************************************
 * Stop playing video
 * @param responce - operation result
 * @param size - max buffer size
 */
void ac_stop_play(char* responce, size_t size);

#endif /* IPCAMTENVIS_AC_RSTP_H */
