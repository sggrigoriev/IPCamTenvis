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

#include "pu_logger.h"

#include "ac_tcp.h"

const char* ac_tcp_read(int sock, char* buf, size_t size) {
    ssize_t rc;
    int full_house = 0;
    char* addr = buf;
    do {
        if ((rc = read(sock, addr, size)) <= 0) {
            pu_log(LL_ERROR, "%s: Read from TCP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
            return NULL;
        }
        if (addr[rc-1]) {  /* Not the full string returned! - the last byte != '\0' */
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
        }
    } while(full_house);
    return buf;
}
int ac_tcp_write(int sock, const char* msg) {
    if(write(sock, msg, strlen(msg)+1) < 0) {
        pu_log(LL_ERROR, "%s: Write to TCP socket failed: RC = %d - %s", __FUNCTION__, errno, strerror(errno));
        return 0;
    }
    return 1;
}

