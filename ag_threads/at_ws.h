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
*/

#ifndef IPCAMTENVIS_AT_WS_H
#define IPCAMTENVIS_AT_WS_H

int start_ws(const char *host, int port, const char *path, const char *session_id);

int is_ws_run();

void stop_ws();

void send_2nd_whisper();

#endif /* IPCAMTENVIS_AT_WS_H */
