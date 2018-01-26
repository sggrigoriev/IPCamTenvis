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
#include <stdint.h>

#include "ab_ring_bufer.h"

#include "ac_cam_types.h"

typedef struct {
    long rc;
    int src;
} t_ac_udp_read_result;

/* Return 0 if error or opened socket if OK
 * NB! use send & recv for the so
 *
*/
int ac_udp_p2p_connection(const char* remote_ip, int remote_port, int home_port);

void ac_close_connection(int sock);

/* Return -1 if error, 0 if timeout, >0 if read smth */
t_ac_udp_read_result ac_udp_read(t_rtsp_pair socks, t_ab_byte* buf, size_t size, int to);
int ac_udp_write(int sock, const t_ab_byte* buf, size_t size);

#endif /* IPCAMTENVIS_AC_UDP_H */
