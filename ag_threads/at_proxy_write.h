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
 Thread to write to Proxy
 Wrapper for queue - TCP interfaces
*/

#ifndef IPCAMTENVIS_AT_PROXY_WRITE_H
#define IPCAMTENVIS_AT_PROXY_WRITE_H

/**
 * Start agent write thread
 *
 * @param wrie_socket   - socket for write
 * @return  - 1
*/
int at_start_proxy_write(int write_socket);

/**
 * Stop agent write thread
*/
void at_stop_proxy_write();

#endif /* IPCAMTENVIS_AT_PROXY_WRITE_H */
