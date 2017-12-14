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
#include <stdio.h>

#include "lib_tcp.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "ag_defaults.h"
#include "at_proxy_rw.h"

#include "at_proxy_write.h"

#define AT_THREAD_NAME "PROXY_WRITE"

/*********************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static char out_buf[LIB_HTTP_MAX_MSG_SIZE]; /* buffer for write into the socket */

static int write_socket;                /* writable socket */
static pu_queue_t* from_main;           /* queue - the source of info to be written into the socket */

/* Thread function: get info from main thread, write it into the socket */
static void* proxy_write(void* params) {
    pu_log(LL_INFO, "%s: started", AT_THREAD_NAME);
    from_main = aq_get_gueue(AQ_ToProxyQueue);

/* Queue events init */
    pu_queue_event_t events;
    events = pu_add_queue_event(pu_create_event_set(), AQ_ToProxyQueue);

/* Main loop */
    while(!at_are_childs_stop()) {
        pu_queue_event_t ev;

        switch(ev=pu_wait_for_queues(events, 1)) {
            case AQ_ToProxyQueue: {
                size_t len = sizeof(out_buf);
                while (pu_queue_pop(from_main, out_buf, &len)) {
                    /* Prepare write operation */
                    ssize_t ret;
                    while(ret = lib_tcp_write(write_socket, out_buf, len, 1), !ret&&!at_are_childs_stop());  /* run until the timeout */
                    if(at_are_childs_stop()) break; /* goto reconnect */
                    if(ret < 0) { /* op start failed */
                        pu_log(LL_ERROR, "%s. Write op failed %d %s. Reconnect", AT_THREAD_NAME, errno, strerror(errno));
/* Put back non-sent message */
                        pu_queue_push(from_main, out_buf, len);
                        at_set_stop_proxy_rw_children();
                        break;
                    }
                    pu_log(LL_DEBUG, "%s: sent: %s", AT_THREAD_NAME, out_buf);
                    len = sizeof(out_buf);
                }
                break;
            }
            case AQ_Timeout:
                break;
            case AQ_STOP:
                at_set_stop_proxy_rw_children();
                pu_log(LL_INFO, "%s: received STOP event. Terminated", AT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait!", AT_THREAD_NAME, ev);
                break;
        }
    }
    pu_log(LL_INFO, "%s is finished", AT_THREAD_NAME);
    at_set_stop_proxy_rw_children();
    lib_tcp_client_close(write_socket);
    pthread_exit(NULL);
}

int at_start_proxy_write(int socket) {
    write_socket = socket;
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &proxy_write, NULL)) return 0;
    return 1;
}

void at_stop_proxy_write() {
    void *ret;

    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    at_set_stop_proxy_rw_children();
}



