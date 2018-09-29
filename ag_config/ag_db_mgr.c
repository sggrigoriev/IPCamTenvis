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
 Created by gsg on 25/09/18.
 Cloud & cam properties db
*/

#include "ag_db_mgr.h"

/* Own properties list
 agent_status: 0/1 -- offline or connected
 ws_status: 0/1
 rw_status: 0/1


wsViewersCount: int
    set when WS sends the "viewersCount"
    Actions:
        from >0 to 0 -> stop streaming
        other changes -> no action
wsPingInterval: int
    set whin WS send the "pingInterval"
wsPongRequest: 0/1
    if 1 - Pong shuould be sent to WS.

 */
