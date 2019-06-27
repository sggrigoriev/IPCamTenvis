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
 Created by gsg on 07/12/17.
 WebSocket IO interface thread
*/

#ifndef IPCAMTENVIS_AT_WS_H
#define IPCAMTENVIS_AT_WS_H

/**
 * Start WS IO thread
 *
 * @param host          - WS host
 * @param port          - WS port
 * @param path          - path in WS host
 * @param session_id    - WS session id
 * @return  - 0 if error, 1 if Ok
 */
int at_ws_start(const char *host, int port, const char *path, const char *session_id);

/**
 * Get the thread status
 * @return  0 thread stop, 1 thread run
 */
int at_is_ws_run();

/**
 * Sync WS IO thread stop (jhoin inside)
 */
void at_ws_stop();

/**
 * Send message to remote WS
 * @param msg   - message to send (string)
 * @return  - 0 if error, 1 if Ok
 */
int at_ws_send(const char* msg);

#endif /* IPCAMTENVIS_AT_WS_H */
