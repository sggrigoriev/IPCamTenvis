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
#include <unistd.h>
#include <errno.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <assert.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "pu_logger.h"
#include "lib_tcp.h"

#include "ac_tcp.h"

const char* at_tcp_get_eth(const char* local_ip, char* eth_name, size_t size) {
    assert(local_ip); assert(eth_name); assert(size>=4);

    eth_name[0] = '\0';
    struct ifaddrs *ifaddr, *ifa;
    int s, n;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        pu_log(LL_ERROR, "%s: error calling 'getifaddrs'. RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return eth_name;
    }

    /* Walk through linked list, maintaining head pointer so we can free list later */
    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if ((ifa->ifa_addr == NULL) || (ifa->ifa_addr->sa_family != AF_INET))
            continue;

        if(s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST), s) {
            pu_log(LL_ERROR, "%s: error calling 'getnameinfo'. RC = %d - %s", __FUNCTION__, errno, strerror(errno));
            goto on_exit;
        }
        if(!strcmp(host, local_ip)) {
            strncpy(eth_name, ifa->ifa_name, size-1);
            goto on_exit;
        }
    }
on_exit:
    freeifaddrs(ifaddr);
    return eth_name;
}

const char* ac_tcp_read(int sock, char* buf, size_t size, int stop) {
    ssize_t rc = 0;
    int full_house = 0;
    char* addr = buf;
    do {
        while(!stop && !rc) {
            rc = read(sock, addr, size);
            if(!rc) sleep(1);   /* timeout */
            if((rc < 0) && ((errno = EAGAIN) || (errno == EWOULDBLOCK))) {
                rc = 0;
                sleep(1);
            }
        }

        if (rc <= 0) {
            pu_log(LL_ERROR, "%s: Read from TCP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
            return NULL;
        }
        if (addr[rc-1] != '\n') {  /* Not the full string returned! - the last byte != '\0' */
            pu_log(LL_WARNING, "%s: Piece of string passed. Read again.", __FUNCTION__);
            full_house = 0;
            addr += rc;
            if(size == rc) {
                pu_log(LL_ERROR, "%s: Read buffer too small. Incoming string truncated", __FUNCTION__);
                addr[rc-1] = '\0';
                break;
            }
            size -= rc;
        }
        else {
            full_house = 1;
            addr[rc] = '\0';
        }
    } while(!full_house);
    return buf;
}

int ac_tcp_write(int sock, t_ab_byte* msg, size_t len, int stop) {
    int again = 1;
    ssize_t rc = -1;
    while(!stop && again) {
        rc = write(sock, msg, len);
        again = (rc < 0) && ((errno = EAGAIN) || (errno == EWOULDBLOCK));
    }
    if(rc < 0) {
        pu_log(LL_ERROR, "%s: Write to TCP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return 0;
    }
    return 1;
}

int ac_tcp_client_connect(const char* ip, int port) {
    int client_socket;
/* Create socket */
    if (client_socket = socket(AF_INET, SOCK_STREAM, 0), client_socket < 0) {
        pu_log(LL_ERROR, "%s: Open TCP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }

    /*Set socket options */
    int32_t on = 1;
    /*use the socket even if the address is busy (by previously killed process for ex) */
    if (setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on)) < 0) {
        pu_log(LL_ERROR, "%s: Open TCP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return -1;
    }

/*Make address */
    struct sockaddr_in addr_struct;
    memset(&addr_struct, 0, sizeof(addr_struct));
    inet_pton(AF_INET, ip, &(addr_struct.sin_addr));

    addr_struct.sin_family = AF_INET;
    addr_struct.sin_port = htons(port);

/*And connect to the remote socket */
    unsigned rpt = LIB_TCP_BINGING_ATTEMPTS;
    while(rpt) {
        int ret = connect(client_socket, (struct sockaddr *)&addr_struct, sizeof(addr_struct));
        if (ret < 0) {
            rpt--;
            sleep(1);   /*wait for a while */
        }
        else {
            int sock_flags = fcntl(client_socket, F_GETFL);
            if (sock_flags < 0) {
                return -1;
            }
            if (fcntl(client_socket, F_SETFL, sock_flags|O_NONBLOCK) < 0) {
                return -1;
            }
            return client_socket;
        }
    }
    pu_log(LL_ERROR, "%s: Connect to TCP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
    return -1;
}

