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
 Created by ml on 06/12/17.
*/


#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <nopoll.h>
#include <nopoll_private.h>

#include "pthread.h"
#include "pu_queue.h"
#include "pu_logger.h"

#include "ag_defaults.h"
#include "ao_cmd_data.h"
#include "ao_cmd_cloud.h"
#include "au_string.h"
#include "aq_queues.h"

#include "at_ws.h"

#define AT_THREAD_NAME  "WS Thread"

static noPollCtx *ctx;
static noPollConn * conn;

static volatile unsigned int viewers_counter;

static pthread_mutex_t io_lock;

static volatile int stop = 1;
static pthread_t ws_thread_id;
static pthread_attr_t threadAttr;

pu_queue_t* from_ws;

static int is_secure_conn(const char* host) {
    char prefix[4]={0};
    int pos = au_findSubstr(host, ":", AU_NOCASE);
    if((pos < 0) || (pos > 3)) return 0;
    strncpy(prefix, host, (size_t)pos);
    prefix[pos] = '\0';
    return (strcasecmp(prefix, "wss")==0);
}
static const char* cut_head(const char* host) {
    int pos = au_findSubstr(host, "://", AU_NOCASE);
    return host + pos + 3;
}
/*
 * this handler fires on every message
*/
static void messageHandler (noPollCtx* ctx, noPollConn* conn, noPollMsg* msg, noPollPtr user_data) {
    char buf[512] = {0};

    size_t len = AU_MIN(msg->payload_size, sizeof(buf)-1);
    memcpy(buf, msg->payload, len);
    buf[len] = '\0';

    pu_log(LL_DEBUG, "%s: From WS: %s, len = %d", AT_THREAD_NAME, buf, len);

    pu_queue_push(from_ws, buf, len+1);
}

static void *ws_thread(void *pvoid) {

    pu_log(LL_DEBUG,"%s: start", AT_THREAD_NAME);
    // wait for messages
    while (!stop) {
        char buf[512];
        int ret = nopoll_loop_wait(ctx, 100);
        switch (ret) {
            case 0:         //No error
            case -3:        //Timeout
                break;
            case -2:
                pu_log(LL_ERROR, "%s: nopoll_loop_wait error - context is NULL or negative TO! Stop", AT_THREAD_NAME, ctx);
                ao_ws_error_answer(buf, sizeof(buf));
                pu_queue_push(from_ws, buf, strlen(buf)+1);
                break;
            case -4:
                pu_log(LL_ERROR, "%s: nopoll_loop_wait error: %d-%s", AT_THREAD_NAME, errno, strerror(errno));
                ao_ws_error_answer(buf, sizeof(buf));
                pu_queue_push(from_ws, buf, strlen(buf)+1);
                break;
            default:
                pu_log(LL_ERROR, "%s: nopoll_loop_wait undrcognized error: %d. Ignored.", AT_THREAD_NAME, ret);
                break;
        }

    }
    pu_log(LL_DEBUG,"%s: exiting\n", AT_THREAD_NAME);

    stop = 1;
    pthread_exit(NULL);
}

int at_ws_start(const char *host, int port,const char *path, const char *session_id) {
    if(at_is_ws_run()) {
        pu_log(LL_ERROR, "WS THREAD already run. Start ignored");
        return 1;
    }
    if(!host || !port || !path || !session_id) {
        pu_log(LL_ERROR, "%s: One of arguments is NULL exiting\n", __FUNCTION__);
        return 0;
    }

    pu_log(LL_INFO, "%s: Starting Web Socket interface...", AT_THREAD_NAME);


// Initiation section
    pthread_mutex_init(&io_lock, NULL);

    pu_log(LL_DEBUG, "%s: host = %s port = %d path = %s session_id = %s", AT_THREAD_NAME, host, port, path, session_id);

    viewers_counter = 0;
    ctx = NULL;
    ctx = nopoll_ctx_new ();
    if (!ctx) {
        pu_log(LL_ERROR, "%s unable to create context",AT_THREAD_NAME);
        goto on_error;
    }

#ifdef NOPOLL_TRACE
    nopoll_log_enable(ctx, nopoll_true);
#else
    nopoll_log_enable(ctx, nopoll_false);
#endif

    from_ws = aq_get_gueue(AQ_FromWS);
    /* call to create a connection */
    //noPollConn * conn = nopoll_conn_new (ctx, "sbox1.presencepro.com", "8080", NULL, "/streaming/camera", NULL, NULL);
    pu_log(LL_DEBUG,"%s: connecting to %s:%d%s", AT_THREAD_NAME, host, port, path);
    char s_port[20];
    sprintf(s_port, "%d", port);
    conn = NULL;
    if(!is_secure_conn(host)) {
        conn = nopoll_conn_new (ctx, cut_head(host), s_port, NULL, path, NULL, NULL);
    }
    else {
        noPollConnOpts* opts = nopoll_conn_opts_new();
// Alternatives: NOPOLL_METHOD_SSLV23, NOPOLL_METHOD_SSLV3, NOPOLL_METHOD_TLSV1
        nopoll_conn_opts_set_ssl_protocol(opts, NOPOLL_METHOD_SSLV23);
        conn = nopoll_conn_tls_new(ctx, opts, cut_head(host), s_port, NULL, path, NULL, NULL);
    }

    if (!nopoll_conn_is_ok (conn)) {
        pu_log(LL_ERROR,"%s: unable to create websocket, errno = %d", AT_THREAD_NAME, errno);
        goto on_error;
    }
    /* wait until connection is ready */
    if (nopoll_conn_wait_until_connection_ready(conn, 5) == nopoll_false) { //todo
        pu_log(LL_ERROR,"%s: failed to connect", AT_THREAD_NAME);
        goto on_error;
    }
/* configure callback */
    nopoll_ctx_set_on_msg (ctx, &messageHandler, NULL);
//Thread function start
    stop = 0;
    pthread_attr_init(&threadAttr);
    pthread_create(&ws_thread_id, &threadAttr, &ws_thread, NULL);

    return 1;
on_error:
    if(conn) nopoll_conn_close(conn);
    if(ctx) nopoll_ctx_unref(ctx);
    pthread_mutex_destroy(&io_lock);
    return 0;
}

void at_ws_stop() {
    void* ret;
    if(!at_is_ws_run()) {
        pu_log(LL_ERROR, "%s is not running. Stop ignored", AT_THREAD_NAME);
        return;
    }
    stop = 1;
    nopoll_loop_stop(ctx);
    pthread_join(ws_thread_id, &ret);
    pthread_attr_destroy(&threadAttr);

    if(conn) nopoll_conn_close(conn);
    if(ctx) nopoll_ctx_unref(ctx);
}

int at_is_ws_run() {
    return !stop;
}

int at_ws_send(const char* msg) {
    assert(msg);
    pu_log(LL_DEBUG, "%s: sending to Web Socket %s", AT_THREAD_NAME, msg);
    long size = (long)strlen(msg)+1;

    pthread_mutex_lock(&io_lock);
        if (nopoll_conn_send_text (conn, msg, size) != size) {
            pu_log(LL_WARNING, "%s:unable to send %s to Web Socket", AT_THREAD_NAME, msg);
        }
    pthread_mutex_unlock(&io_lock);

    return 1;
}

unsigned int at_ws_get_active_viewers_amount() {
    return viewers_counter;
}
void at_ws_set_active_viewers_amount(unsigned int amount) {
    viewers_counter = amount;
}
