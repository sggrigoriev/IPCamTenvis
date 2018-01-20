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
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>

#include "pu_logger.h"

#include "ab_ring_bufer.h"

#include "ac_udp.h"

int ac_udp_client_connection(const char* ip, uint16_t port, struct sockaddr_in* sin, int async) {
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
    if(async) {
        int sock_flags = fcntl(ret, F_GETFL);
        if (sock_flags < 0) {
            return -1;
        }
        if (fcntl(ret, F_SETFL, sock_flags | O_NONBLOCK) < 0) {
            return -1;
        }
    }
    return ret;
}
int ac_udp_server_connection(const char* my_ip, uint16_t my_port, const char* other_ip, uint16_t other_port, struct sockaddr_in* smy, struct sockaddr_in* sother, int async) {
    int ret = -1;

    pu_log(LL_DEBUG, "%s: source ip = %s, source port = %d, dest ip = %s, dest port = %d", __FUNCTION__, other_ip, other_port, my_ip, my_port);

    memset(smy, 0, sizeof(struct sockaddr_in));
    smy->sin_family = AF_INET;
    smy->sin_addr.s_addr = inet_addr(my_ip);
    smy->sin_port = htons(my_port);

    memset(sother, 0, sizeof(struct sockaddr_in));
    sother->sin_family = AF_INET;
    sother->sin_addr.s_addr = inet_addr(other_ip);
    sother->sin_port = htons(other_port);

    if((ret=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        pu_log(LL_ERROR, "%s: Open UDP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }
    if(async) {
        int sock_flags = fcntl(ret, F_GETFL);
        if (sock_flags < 0) {
            return -1;
        }
        if (fcntl(ret, F_SETFL, sock_flags | O_NONBLOCK) < 0) {
            return -1;
        }
    }
    int rc_bind;
    if (rc_bind = bind(ret, (const struct sockaddr *)smy, sizeof(struct sockaddr_in)), rc_bind < 0) {
        pu_log(LL_ERROR, "%s: Binding UDP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
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
ssize_t ac_udp_read(int sock, struct sockaddr_in* sother, t_ab_byte* buf, size_t size, int to) {
    ssize_t res;

/*Build set for select */
    struct timeval tv = {to, 0};
    fd_set readset;
    FD_ZERO(&readset);

    FD_SET(sock, &readset);

    if(sock < 0) {
        pu_log(LL_ERROR, "%s: FD_SET error: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }
    res = select(sock + 1, &readset, NULL, NULL, &tv);
    if(res < 0) {    /* Error. nothing to read */
        pu_log(LL_ERROR, "%s: select error: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }
    if(res == 0) return 0;   /*timeout */
    socklen_t slen = sizeof(struct sockaddr_in);
    if((res = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)sother, &slen) < 0)) {
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
