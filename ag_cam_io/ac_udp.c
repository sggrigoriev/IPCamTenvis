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
#include <memory.h>
#include <errno.h>
#include<sys/poll.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include "pu_logger.h"

#include "ab_ring_bufer.h"

#include "ac_udp.h"

int ac_udp_server_connecion(const char* ip, uint16_t port) {
    int ret = -1;
    struct sockaddr_in sin={0};

    pu_log(LL_DEBUG, "%s: ip = %s, port = %d", __FUNCTION__, ip, port);

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(ip);
    sin.sin_port=htons(port);

    if((ret=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        pu_log(LL_ERROR, "%s: Open UDP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }
    if(bind(ret,(struct sockaddr*)&sin,sizeof(sin))==-1) {
        pu_log(LL_ERROR, "%s: UDP socket bind failed: RC = %d - %s",  __FUNCTION__, errno, strerror(errno));
        return -1;
    }
    return ret;
}
int ac_udp_client_connection(const char* ip, uint16_t port, struct sockaddr_in* sin) {
    int ret = -1;

    pu_log(LL_DEBUG, "%s: ip = %s, port = %d", __FUNCTION__, ip, port);

    memset(sin, 0, sizeof(struct sockaddr_in));

    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = inet_addr(ip);
    sin->sin_port = htons(port);

    if((ret=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        pu_log(LL_ERROR, "%s: Open UDP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }
    return ret;
}
void ac_close_connection(int sock) {
    if(sock > 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
}

/* Return -1 if error, 0 if timeout, >0 if read smth */
ssize_t ac_udp_read(int sock, t_ab_byte* buf, size_t size, int to) {
    struct pollfd fd;
    ssize_t res;

    fd.fd = sock;
    fd.events = POLLIN;
    res = poll(&fd, 1, to*1000); /* timeout in ms */

    if (res == 0) {
        return 0;
    }
    else if (res == -1) {
        pu_log(LL_ERROR, "%s: Polling UDP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }
    else if((res = recv(sock, buf, sizeof(buf), 0) < 0)) {  /* implies (fd.revents & POLLIN) != 0 */
        pu_log(LL_ERROR, "%s: Read UDP socket error: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
    }
    if(res == size) {
        pu_log(LL_WARNING, "%s: Read buffer too small - data truncated!", __FUNCTION__);
    }
    return res;
}
int ac_udp_write(int sock, const t_ab_byte* buf, size_t size, const struct sockaddr_in* addr) {
    if(sendto(sock, buf, size, 0, (const struct sockaddr *)addr, sizeof(addr)) < 0) {
        pu_log(LL_ERROR, "%s: Write on UDP socket error: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return 0;
    }
    return 1;
}
