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
#include <assert.h>
#include <sys/select.h>
#include <netdb.h>
#include <time.h>
#include <asm/errno.h>


#include "pu_logger.h"

#include "ab_ring_bufer.h"
#include "ac_cam_types.h"
#include "ac_udp.h"

#define AC_UDP_MAX(a,b) ((a>b)?a:b)
/*
static const char* get_ip_from_sock(int sock, char* ip, size_t ip_size) {
    struct sockaddr_in addr;
    ip[0] = '\0';
    socklen_t len = sizeof(struct sockaddr_in);
    int ret = getsockname(sock, (struct sockaddr*)&addr, &len);
    if(!ret)
        inet_ntop(AF_INET, &addr.sin_addr, ip, ip_size);
    return ip;
}
*/
/* Copypizded from https://stackoverflow.com/questions/9741392/can-you-bind-and-connect-both-ends-of-a-udp-connection */
int ac_udp_p2p_connection(const char* remote_ip, int remote_port, int home_port) {
    int sockfd = -1;
    struct addrinfo hints, *remote_info = NULL, *home_info = NULL, *p = NULL;
    int ret = 0;

    memset(&hints, 0, sizeof hints);
#if 0
    hints.ai_family = AF_UNSPEC;
#else
    hints.ai_family = AF_INET;
#endif

    hints.ai_socktype = SOCK_DGRAM;        //UDP communication

    /*For remote address*/
    int rc = -1;
    char asc_port[10];
    snprintf(asc_port, sizeof(asc_port)-1, "%d", remote_port);
    if ((rc = getaddrinfo(remote_ip, asc_port, &hints, &remote_info)) != 0) {
        pu_log(LL_ERROR, "%s: getaddr info for remote peer: %s", __FUNCTION__, gai_strerror(rc));
        goto on_exit;
    }

    // loop through all the results and make a socket
    for(p = remote_info; p != NULL; p = p->ai_next) {
#if 0
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
#else
        if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
#endif
                    pu_log(LL_ERROR, "%s: Error open socket %s - %d", __FUNCTION__, strerror(errno), errno);
            continue;
        }
        /*Taking first entry from getaddrinfo*/
        break;
    }
    /*Failed to get socket to all entries*/
    if (p == NULL) {
        pu_log(LL_ERROR, "%s: Failed to get socket", __FUNCTION__);
        goto on_exit;
    }

    /*For home address*/
    memset(&hints, 0, sizeof hints);
#if 0
    hints.ai_family = AF_UNSPEC;
#else
    hints.ai_family = AF_INET;
#endif
    hints.ai_socktype = SOCK_DGRAM;  //UDP communication
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    snprintf(asc_port, sizeof(asc_port)-1, "%d", home_port);
    if ((rc = getaddrinfo(NULL, asc_port, &hints, &home_info)) != 0) {
        pu_log(LL_ERROR, "%s: getaddr info for home peer: %s - %d", __FUNCTION__, strerror(errno), errno);
        goto on_exit;
    }

    int32_t on = 1;
    /*use the socket even if the address is busy (by previously killed process for ex) */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on)) < 0) {
        pu_log(LL_ERROR, "%s: setsockopt error: %s - %d", __FUNCTION__, strerror(errno), errno);
        goto on_exit;
    }
/*
    // Set the socket as async
    int sock_flags = fcntl(sockfd, F_GETFL);
    if (sock_flags < 0) {
        pu_log(LL_ERROR, "%s: fcntl Get Flags error: %s - %d", __FUNCTION__, strerror(errno), errno);
        goto on_exit;
    }

    if (fcntl(sockfd, F_SETFL, sock_flags|O_NONBLOCK) < 0) {
        pu_log(LL_ERROR, "%s: fcntl Set Flags error: %s - %d", __FUNCTION__, strerror(errno), errno);
        goto on_exit;
    }
*/
    /*Bind this datagram socket to home address info */
    if((rc = bind(sockfd, home_info->ai_addr, home_info->ai_addrlen)) != 0) {
        pu_log(LL_ERROR, "%s: Error bind socket: %s - %d", __FUNCTION__, strerror(errno), errno);
        goto on_exit;
    }

    /*Connect this datagram socket to remote address info */
    if(connect(sockfd, p->ai_addr, p->ai_addrlen) != 0) {
        pu_log(LL_ERROR, "%s: Error connect socket %s - %d", __FUNCTION__, strerror(errno), errno);
        goto on_exit;
    }
    ret = 1;
on_exit:
    if(remote_info)
        freeaddrinfo(remote_info);

    if(home_info)
        freeaddrinfo(home_info);

    return (ret)?sockfd:-1;
}

void ac_udp_close_connection(int sock) {
    if(sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
}

/* Return -1 if error, 0 if timeout, >0 if read smth */
t_ac_udp_read_result ac_udp_read(int sock, t_ab_byte* buf, size_t size, int to) {

    t_ac_udp_read_result rc={-1, 0};

    struct timespec t = {1,99999999}, rem;
    useconds_t tm = 100000;

    if(rc.rc = read(sock, buf, size), rc.rc < 0) {

        if(errno == ECONNREFUSED) pu_log(LL_ERROR, "%s: Connection refuzed", __FUNCTION__);
        if((errno == ECONNREFUSED) || (errno == EAGAIN)) {

            usleep(tm);

/*
            if(nanosleep(&t, &rem)) {
                pu_log(LL_ERROR, "%s: nanosleep returns %d", __FUNCTION__, errno);
                rc.rc = -1;
            }
*/
            rc.rc = 0; //Let it try again
        }
        else {
            pu_log(LL_ERROR, "%s: Read socket error: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        }
    }
    if(rc.rc == size) {
        pu_log(LL_WARNING, "%s: Read buffer %d is too small - data truncated!", __FUNCTION__, size);
    }

    return rc;
}
int ac_udp_write(int sock, const t_ab_byte* buf, size_t size) {
    struct timespec t = {0,100}, rem;
    long rc = 0;

    while(!rc) {
        if (rc = write(sock, buf, size), rc < 0) {
            if ((errno == ECONNREFUSED) || (errno == EAGAIN)) {
                nanosleep(&t, &rem);
                rc = 0; //Let it try again
            }
            else {
                pu_log(LL_ERROR, "%s: Write on UDP socket error: RC = %d - %s. Buffer size = %d", __FUNCTION__, errno, strerror(errno), size);
                return 0;
            }
        }
    }
    return 1;
}
