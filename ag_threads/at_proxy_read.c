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
#include <ag_config/ag_defaults.h>

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

int read_proxy_socket = -1;                              /* socket to read from Proxy */
int read_mon_socket = -1;                                /* socket to read from camera events monitor */

static pu_queue_t* from_proxy;             /* main queue pointer - local transport */
static pu_queue_t* from_mon;

/* Thread function. Reads info, assemble it to the buffer and forward to the main thread by queue */
static void* proxy_read(void* params) {
    pu_log(LL_INFO, "%s started", AT_THREAD_NAME);
    from_proxy = aq_get_gueue(AQ_FromProxyQueue);
    from_mon = aq_get_gueue(AQ_FromCam);
    lib_tcp_rd_t *proxy_conn = NULL;
    lib_tcp_rd_t *mon_conn = NULL;

    lib_tcp_conn_t* all_conns = lib_tcp_init_conns(2, LIB_HTTP_MAX_MSG_SIZE-LIB_HTTP_HEADER_SIZE);
    if(!all_conns) {
        pu_log(LL_ERROR, "%s: memory allocation error.", AT_THREAD_NAME);
        goto allez;      /* Allez kaputt */
    }
    if(proxy_conn = lib_tcp_add_new_conn(read_proxy_socket, all_conns), !proxy_conn) {
        pu_log(LL_ERROR, "%s: new incoming connection exeeds max amount. Aborted", AT_THREAD_NAME);
         goto allez;
    }

monitor_connect:
    if(read_mon_socket = lib_tcp_get_client_socket(DEFAULT_CAM_MON_PORT, 1), read_mon_socket > 0) {
        mon_conn = lib_tcp_add_new_conn(read_mon_socket, all_conns);
        if(!mon_conn) {
            pu_log(LL_ERROR, "%s: new incoming connection exeeds max amount. Aborted", AT_THREAD_NAME);
            goto allez;
        }
        pu_log(LL_INFO, "%s: EM connected by socket# %d", AT_THREAD_NAME, read_mon_socket);
    }
    else {
        pu_log(LL_WARNING, "%s: Connection to EM failed. Reconnect.", AT_THREAD_NAME);
    }
    while(!at_are_childs_stop()) {
        int rc;
        lib_tcp_rd_t *conn = lib_tcp_read(all_conns, 1, &rc); /* connection removed inside */
        if(rc == LIB_TCP_READ_EOF) {
            pu_log(LL_ERROR, "%s. Read op failed. Nobody on remote side (EOF). Reconnect", AT_THREAD_NAME);
            goto allez;
        }
        if (rc == LIB_TCP_READ_TIMEOUT) {
            if(read_mon_socket < 0) {
                pu_log(LL_WARNING, "%s: No connection to Camera Events Monitor. Reconnect", AT_THREAD_NAME);
                goto monitor_connect;
            }
            continue;   /* timeout */
        }
        if (rc == LIB_TCP_READ_MSG_TOO_LONG) {
            pu_log(LL_ERROR, "%s: incoming mesage too large. Ignored", AT_THREAD_NAME);
            continue;
        }
        if (rc == LIB_TCP_READ_NO_READY_CONNS) {
            pu_log(LL_ERROR, "%s: internal error - no ready sockets. Reconnect", AT_THREAD_NAME);
            goto allez;
        }
        if (!conn) {
            pu_log(LL_ERROR, "%s. Undefined error - connection not found. Reconnect", AT_THREAD_NAME);
            goto allez;
        }

        pu_queue_t* q = (conn->socket == read_proxy_socket)?from_proxy:from_mon;
        while (lib_tcp_assemble(conn, out_buf, sizeof(out_buf))) {     /* Read all fully incoming messages */
            pu_queue_push(q, out_buf, strlen(out_buf) + 1);
            pu_log(LL_INFO, "%s: message received: %s", AT_THREAD_NAME, out_buf);
        }
    }
allez:
/* if mom connection error - restart monitor connection */
    if(proxy_conn && proxy_conn->socket > 0) {
        pu_log(LL_WARNING, "%s: Lost connection to Event Monitor", __FUNCTION__);
        goto monitor_connect;
    }

    at_set_stop_proxy_rw_children();
    lib_tcp_destroy_conns(all_conns);
    pu_log(LL_INFO, "%s is finished", AT_THREAD_NAME);
    pthread_exit(NULL);
}


int at_start_proxy_read(int socket) {
    read_proxy_socket = socket;
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &proxy_read, &read_proxy_socket)) return 0;
    return 1;
}

void at_stop_proxy_read() {
    void *ret;

    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
    at_set_stop_proxy_rw_children();
}


