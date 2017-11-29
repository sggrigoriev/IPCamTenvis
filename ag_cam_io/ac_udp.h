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

#ifndef IPCAMTENVIS_AC_UDP_H
#define IPCAMTENVIS_AC_UDP_H

#include <stdio.h>

/* Return -1 if error or opened socket if OK */
int ac_udp_server_connecion(const char* ip, uint16_t port);
int ac_udp_client_connection(const char* ip, uint16_t port, struct sockaddr_in* sin);
void ac_close_connection(int socket);

/* Return -1 if error, 0 if timeout, >0 if read smth */
ssize_t ac_udp_read(int socket, t_ab_byte* buf, size_t size, int to);
int ac_udp_write(int socket, const t_ab_byte* buf, size_t size, const struct sockaddr_in* addr);

#endif /* IPCAMTENVIS_AC_UDP_H */
