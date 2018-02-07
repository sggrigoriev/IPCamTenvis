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
    hints.ai_family = AF_UNSPEC;
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
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
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
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;  //UDP communication
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    snprintf(asc_port, sizeof(asc_port)-1, "%d", home_port);
    if ((rc = getaddrinfo(NULL, asc_port, &hints, &home_info)) != 0) {
        pu_log(LL_ERROR, "%s: getaddr info for home peer: %s", __FUNCTION__, gai_strerror(rc));
        goto on_exit;
    }

    // Set the socket as async
    int sock_flags = fcntl(sockfd, F_GETFL);
    if (sock_flags < 0) {
        return -1;
    }
    if (fcntl(sockfd, F_SETFL, sock_flags|O_NONBLOCK) < 0) {
        return -1;
    }

    /*Bind this datagram socket to home address info */
    if((rc = bind(sockfd, home_info->ai_addr, home_info->ai_addrlen)) != 0) {
        pu_log(LL_ERROR, "%s: Error bind socket %s", __FUNCTION__, gai_strerror(rc));
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

void ac_close_connection(int sock) {
    if(sock > 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
}

/* Return -1 if error, 0 if timeout, >0 if read smth */
t_ac_udp_read_result ac_udp_read(t_rtsp_pair socks, t_ab_byte* buf, size_t size, int to) {

    t_ac_udp_read_result rc={-1,0};

// Build set for select
    struct timeval tv = {to, 0};
    fd_set readset;

    FD_ZERO(&readset);

    FD_SET(socks.rtp, &readset);
    FD_SET(socks.rtcp, &readset);

    rc.rc = select(AC_UDP_MAX(socks.rtcp, socks.rtp) + 1, &readset, NULL, NULL, &tv);
    if(rc.rc < 0) {    // Error. nothing to read
        pu_log(LL_ERROR, "%s: select error: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return rc;
    }
    if(rc.rc == 0) return rc; // timeout

    int sock;
    if(FD_ISSET(socks.rtcp, &readset)) {
        rc.src = 0;
        sock = socks.rtcp;
        pu_log(LL_DEBUG, "BINGO!!! Got RTCP message!");
    }
    else {
        rc.src = 1;
        sock = socks.rtp;
    }

    if(rc.rc = read(sock, buf, size), rc.rc < 0) {
        pu_log(LL_ERROR, "%s: Read UDP socket error: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        if(errno == ECONNREFUSED) rc.rc = 0; //Let it try again
    }
    if(rc.rc == size) {
        pu_log(LL_WARNING, "%s: Read buffer too small - data truncated!", __FUNCTION__);
    }

    return rc;
}
int ac_udp_write(int sock, const t_ab_byte* buf, size_t size) {
    if(write(sock, buf, size) < 0) {
        pu_log(LL_ERROR, "%s: Write on UDP socket error: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return 0;
    }
    return 1;
}
