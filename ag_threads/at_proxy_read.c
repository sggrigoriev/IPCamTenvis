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
*/
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "lib_tcp.h"
#include "lib_http.h"
#include "pu_logger.h"
#include "aq_queues.h"

#include "at_proxy_rw.h"
#include "at_proxy_read.h"

#define AT_THREAD_NAME "PROXY_READ"

/***************************************************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static char out_buf[LIB_HTTP_MAX_MSG_SIZE] = {0};   /* buffer to receive the data */

int read_socket;                            /* socket to read from  */
static pu_queue_t* from_proxy;             /* main queue pointer - local transport */

/* Thread function. Reads info, assemble it to the buffer and forward to the main thread by queue */
static void* proxy_read(void* params) {
    pu_log(LL_INFO, "%s started", AT_THREAD_NAME);
    from_proxy = aq_get_gueue(AQ_FromProxyQueue);

    lib_tcp_conn_t* all_conns = lib_tcp_init_conns(1, LIB_HTTP_MAX_MSG_SIZE-LIB_HTTP_HEADER_SIZE);
    if(!all_conns) {
        pu_log(LL_ERROR, "%s: memory allocation error.", AT_THREAD_NAME);
        at_set_stop_proxy_rw_children();
        goto allez;      /* Allez kaputt */
    }
    if(!lib_tcp_add_new_conn(read_socket, all_conns)) {
        pu_log(LL_ERROR, "%s: new incoming connection exeeds max amount. Aborted", AT_THREAD_NAME);
        at_set_stop_proxy_rw_children();
        goto allez;
    }
    sleep(5);
    while(!at_are_childs_stop()) {
        int rc;
        lib_tcp_rd_t *conn = lib_tcp_read(all_conns, 1, &rc); /* connection removed inside */
        if(rc == LIB_TCP_READ_EOF) {
            pu_log(LL_ERROR, "%s. Read op failed. Nobody on remote side (EOF). Reconnect", AT_THREAD_NAME);
            at_set_stop_proxy_rw_children();
            break;
        }
        if (rc == LIB_TCP_READ_TIMEOUT) {
//            pu_log(LL_DEBUG, "%s: timeout", AT_THREAD_NAME);
            continue;   /* timeout */
        }
        if (rc == LIB_TCP_READ_MSG_TOO_LONG) {
            pu_log(LL_ERROR, "%s: incoming mesage too large. Ignored", AT_THREAD_NAME);
            continue;
        }
        if (rc == LIB_TCP_READ_NO_READY_CONNS) {
            pu_log(LL_ERROR, "%s: internal error - no ready sockets. Reconnect", AT_THREAD_NAME);
            at_set_stop_proxy_rw_children();
            break;
        }
        if (!conn) {
            pu_log(LL_ERROR, "%s. Undefined error - connection not found. Reconnect", AT_THREAD_NAME);
            at_set_stop_proxy_rw_children();
            break;
        }

        while (lib_tcp_assemble(conn, out_buf, sizeof(out_buf))) {     /* Read all fully incoming messages */
            pu_queue_push(from_proxy, out_buf, strlen(out_buf) + 1);
            pu_log(LL_INFO, "%s: message received: %s", AT_THREAD_NAME, out_buf);
        }
    }
    allez:
    at_set_stop_proxy_rw_children();
    lib_tcp_destroy_conns(all_conns);
    pu_log(LL_INFO, "%s is finished", AT_THREAD_NAME);
    pthread_exit(NULL);
}


int at_start_proxy_read(int socket) {
    read_socket = socket;
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &proxy_read, &read_socket)) return 0;
    return 1;
}

void at_stop_proxy_read() {
    void *ret;

    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
    at_set_stop_proxy_rw_children();
}


