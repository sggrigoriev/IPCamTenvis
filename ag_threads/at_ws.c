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

#include <nopoll.h>
#include <nopoll_private.h>
#include <pthread.h>
#include <ag_config/ag_defaults.h>
#include <ag_converter/ao_cmd_data.h>
#include <ag_converter/ao_cmd_cloud.h>
#include <assert.h>

#include "pu_queue.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "at_ws.h"

#define AT_THREAD_NAME  "WS Thread"

static noPollCtx *ctx;
static noPollConn * conn;

static pthread_mutex_t lock;

static volatile int stop = 1;
static pthread_t ws_thread_id;
static pthread_attr_t threadAttr;

pu_queue_t* from_ws;

/*
 * this handler fires on every message
*/
static void messageHandler (noPollCtx* ctx, noPollConn* conn, noPollMsg* msg, noPollPtr user_data) {
    const char ping[3] = "{}";
    t_ao_msg data;
    t_ao_msg_type msg_type;

    pu_log(LL_DEBUG, "%s: From WS: %s", AT_THREAD_NAME, msg->payload);

    msg_type = ao_cloud_decode(msg->payload, &data);
    if(msg_type != AO_WS_ANSWER) {
        pu_log(LL_ERROR, "%s: received unrecognized message. Ignored", AT_THREAD_NAME);
        return;
    }
    if(data.ws_answer.ws_msg_type == AO_WS_PING) {
        at_ws_send(ping);
    }
    else {
        pu_queue_push(from_ws, msg->payload, (size_t)msg->payload_size);
     }
}

static void *ws_thread(void *pvoid) {

    pu_log(LL_DEBUG,"%s: start", AT_THREAD_NAME);
    // wait for messages
    while (!stop) {
        nopoll_loop_wait(ctx, 10);
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
    pthread_mutex_init(&lock, NULL);

    pu_log(LL_DEBUG, "%s: host = %s port = %d path = %s session_id = %s", AT_THREAD_NAME, host, port, path, session_id);

    ctx = NULL;
    noPollCtx *ctx = nopoll_ctx_new ();
    nopoll_log_enable(ctx, nopoll_false);
    if (!ctx) {
        pu_log(LL_ERROR, "%s unable to create context",AT_THREAD_NAME);
        goto on_error;
    }
    from_ws = aq_get_gueue(AQ_FromWS);
    /* call to create a connection */
    //noPollConn * conn = nopoll_conn_new (ctx, "sbox1.presencepro.com", "8080", NULL, "/streaming/camera", NULL, NULL);
    pu_log(LL_DEBUG,"%s: connecting to %s:%d%s", AT_THREAD_NAME, host, port, path);
    char s_port[20];
    sprintf(s_port, "%d", port);
    conn = NULL;
    conn = nopoll_conn_new (ctx, host, s_port, NULL, path, NULL, NULL);

    if (!nopoll_conn_is_ok (conn)) {
        pu_log(LL_ERROR,"%s: unable to create websocket", AT_THREAD_NAME);
        goto on_error;
    }
    /* wait until connection is ready */
    if (!nopoll_conn_wait_until_connection_ready(conn, 5) ) { //todo
        pu_log(LL_ERROR,"%s: failed to connect", AT_THREAD_NAME);
        goto on_error;
    }
    /* Cam connection request */
    char buf[512] = {0};
    at_ws_send(ao_connection_request(buf, sizeof(buf), session_id));

    /* configure callback */
    nopoll_ctx_set_on_msg (ctx, messageHandler, NULL);
//Thread function start
    stop = 0;
    pthread_attr_init(&threadAttr);
    pthread_create(&ws_thread_id, &threadAttr, &ws_thread, NULL);

    return 1;
on_error:
    if(conn) nopoll_conn_close(conn);
    if(ctx) nopoll_ctx_unref(ctx);
    return 0;
}

void at_ws_stop() {
    void* ret;
    if(!at_is_ws_run()) {
        pu_log(LL_ERROR, "%s is not running. Stop ignored", AT_THREAD_NAME);
        return;
    }
    stop = 1;
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
    pthread_mutex_lock(&lock);

    if (nopoll_conn_send_text (conn, msg, size) != size) {
        pu_log(LL_WARNING, "%s:unable to send %s to Web Socket", AT_THREAD_NAME, msg);
    }
    pthread_mutex_unlock(&lock);

    return 1;
}

