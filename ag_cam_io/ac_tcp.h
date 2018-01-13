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
 Created by gsg on 29/11/17.
*/

#ifndef IPCAMTENVIS_AC_TCP_H
#define IPCAMTENVIS_AC_TCP_H

#include <stdio.h>

const char* ac_tcp_read(int sock, char* buf, size_t size, int stop);
int ac_tcp_write(int sock, const char* msg, int stop);

int ac_tcp_client_connect(const char* ip, int port);
#endif /* IPCAMTENVIS_AC_TCP_H */