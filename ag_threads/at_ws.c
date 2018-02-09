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

#include "pu_queue.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "at_ws.h"

typedef struct _ws_args {
    char hostname[128];
    char port[6];
    char path[128];
    char session_id[128];
} ws_args;

static volatile int send_3rd = 0;

static volatile int stop = 1;
static pthread_t ws_thread_id;
pu_queue_t* from_ws;
/* this handler fires on every message
 * {"resultCode":0,"params":[{"name":"ppc.streamStatus","setValue":"1","forward":0}],"viewers":[{"id":"17","status":1}]} <-- should not respond
 * messageHandler {"resultCode":10} <-- should respond "{}"
 *
 * */
static int start_connector(const char* msg) {
    if(strstr(msg, "\"setValue\":\"1\"")) return 1;
    if(strstr(msg, "\"setValue\":\"0\"")) return 0;
    pu_log(LL_ERROR, "%s: Can't find \"set value\" token. Message ignored", __FUNCTION__);
    return -1;
}
static void messageHandler (noPollCtx  * ctx,
                                noPollConn * conn,
                                noPollMsg  * msg,
                                noPollPtr    user_data)
{
    pu_log(LL_DEBUG, "%s %s\n",__FUNCTION__,(char*)(msg->payload));
    char ping[3] = "{}\n";

    if(strstr((char*)(msg->payload),":10")!=NULL) {
        if (nopoll_conn_send_text (conn, ping, 2) != 2) {
            pu_log(LL_WARNING, "%s:unable to send ping respose\n",__FUNCTION__);
        }
    }
    else {
         switch(start_connector(msg->payload)) {
            case 0:
                pu_queue_push(from_ws, DEFAULT_WC_STOP_PLAY, strlen(DEFAULT_WC_STOP_PLAY)+1);
                 pu_log(LL_DEBUG, "%s: send STOP", __FUNCTION__);
                break;
             case 1:
                pu_queue_push(from_ws, DEFAULT_WC_START_PLAY, strlen(DEFAULT_WC_START_PLAY)+1);
                 pu_log(LL_DEBUG, "%s: send START", __FUNCTION__);
                 send_3rd = 1;
                break;
             default:
                 break;
        }

    }

}
static void *ws_read_thread(void *pvoid) {

    char buff[512];
    ws_args *args;

    args = (ws_args*)pvoid;

    pu_log(LL_DEBUG, "%s: args->hostname = %s args->port = %s args->path = %s args->session_id = %s", __FUNCTION__, args->hostname, args->port, args->path, args->session_id);

    noPollCtx *ctx = nopoll_ctx_new ();
    nopoll_log_enable(ctx,nopoll_false);
    if (!ctx) {
        pu_log(LL_ERROR, "%s unable to create context",__FUNCTION__);
    }
    from_ws = aq_get_gueue(AQ_FromWS);

    /* call to create a connection */
    //noPollConn * conn = nopoll_conn_new (ctx, "sbox1.presencepro.com", "8080", NULL, "/streaming/camera", NULL, NULL);
    pu_log(LL_DEBUG,"%s: connecting to %s:%s%s",__FUNCTION__,args->hostname,args->port,args->path);
    noPollConn * conn = nopoll_conn_new (ctx, args->hostname, args->port, NULL, args->path, NULL, NULL);

    if (!nopoll_conn_is_ok (conn)) {
        pu_log(LL_ERROR,"%s: unable to create websocket",__FUNCTION__);
    }
    /* wait until connection is ready */
    if (!nopoll_conn_wait_until_connection_ready(conn, 5) ) { //todo
        pu_log(LL_ERROR,"%s: failed to connect",__FUNCTION__);
        stop = 1;
        return (0);
    }
    /* send 1's magic whisper */
    size_t n = sprintf( (char *)buff, "{\"sessionId\": \"%s\"}", args->session_id);
    pu_log(LL_DEBUG, "%s: To Web Socket-1: %s", "WS_THREAD", buff);
    if (nopoll_conn_send_text (conn, buff, n) != n) {
        pu_log(LL_ERROR,"%s: failed to send 1",__FUNCTION__);
    }
    /* send 2'nd magic whisper */
    n = sprintf( (char *)buff, "{\"params\":[{\"name\":\"ppc.streamStatus\", \"value\":\"%s\"}]}", args->session_id);
    pu_log(LL_DEBUG, "%s: To Web Socket-2: %s", "WS_THREAD", buff);
    if (nopoll_conn_send_text (conn, buff, n) != n) {
        pu_log(LL_ERROR,"%s: failed to send 2",__FUNCTION__);
    }
    /* configure callback */
    nopoll_ctx_set_on_msg (ctx, messageHandler, NULL);
    // wait for messages
    while (!stop) {
        nopoll_loop_wait(ctx, 10);
/*
        if(send_3rd) {
            send_3rd = 0;
            n = sprintf((char*)buff, "{\"sessionId\":\"%s\",\"params\":[{\"name\":\"ppc.streamStatus\",\"setValue\":\"%s\",\"forward\":1}]}", args->session_id, args->session_id);
            pu_log(LL_DEBUG, "%s: To Web Socket after START command received: %s", "WS_THREAD", buff);
            if (nopoll_conn_send_text (conn, buff, n) != n) {
                pu_log(LL_ERROR,"%s: failed to send ",__FUNCTION__);
            }
        }
*/
     }
    pu_log(LL_DEBUG,"%s: exiting\n", __FUNCTION__);

    nopoll_conn_close(conn);
    stop = 1;
    pthread_exit(NULL);
}

pthread_attr_t threadAttr;

int start_ws(const char *host, int port,const char *path, const char *session_id) {

    ws_args *argz;
    char s_port[20];

    if(is_ws_run()) {
        pu_log(LL_ERROR, "WS THREAD already run. Start ignored");
        return 1;
    }

    if(!host || !port || !path || !session_id) {
        pu_log(LL_ERROR, "%s: One of arguments is NULL exiting\n", __FUNCTION__);
        return 0;
    }
    sprintf(s_port, "%d", port);
    argz = calloc(1,sizeof(ws_args));
    strcpy(argz->hostname,host);
    strcpy(argz->port,s_port);
    strcpy(argz->path,path);
    strcpy(argz->session_id,session_id);

    pu_log(LL_INFO, "%s: Starting Web Socket interface...",__FUNCTION__);

    stop = 0;
    pthread_attr_init(&threadAttr);
    pthread_create(&ws_thread_id, &threadAttr, &ws_read_thread, (void*)argz);

    return 1;
}

void stop_ws()
{
    void* ret;
    if(!is_ws_run()) {
        pu_log(LL_ERROR, "WS THERAD is not running. Stop ignored");
        return;
    }
    stop = 1;
    pthread_join(ws_thread_id, &ret);
    pthread_attr_destroy(&threadAttr);
}

int is_ws_run() {
    return !stop;
}




