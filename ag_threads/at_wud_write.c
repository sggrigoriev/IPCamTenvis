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
 Created by gsg on 15/12/17.
*/

#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <ag_config/ag_settings.h>

#include "lib_http.h"
#include "lib_tcp.h"

#include "aq_queues.h"
#include "at_wud_write.h"

#define PT_THREAD_NAME "WUD_WRITE"

/******************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static volatile int stop=1;                   /* Thread stop flag */

static char out_buf[LIB_HTTP_MAX_MSG_SIZE*2];   /* bufffer for sending data */

static pu_queue_t* to_wud;                  /* transport here */

/**********************************************************************
 * Local functions
 */

/* Main thread function */
static void* wud_write(void* params) {
    to_wud = aq_get_gueue(AQ_ToWUD);

/* Queue events init */
    pu_queue_event_t events;
    events = pu_add_queue_event(pu_create_event_set(), AQ_ToWUD);

    while(!stop) {
        int write_socket = -1;

        while(write_socket = lib_tcp_get_client_socket(ag_getWUDPort(), 1), write_socket <= 0) {
            pu_log(LL_ERROR, "%s: connection error %d %s", PT_THREAD_NAME, errno, strerror(errno));
            sleep(1);
            continue;
        }
        pu_log(LL_DEBUG, "%s: Connected. Port = %d, socket = %d", PT_THREAD_NAME, ag_getWUDPort(), write_socket);

        int reconnect = 0;
        while (!stop) {
            pu_queue_event_t ev;
            switch (ev = pu_wait_for_queues(events, 1)) {
                case AQ_ToWUD: {
                    size_t len = sizeof(out_buf);
                    while (pu_queue_pop(to_wud, out_buf, &len)) {
                        ssize_t ret;
                        while(ret = lib_tcp_write(write_socket, out_buf, len, 1), !ret&&!stop);  /* run until the timeout */
                        if(stop) break; /* goto reconnect */
                        if(ret < 0) {   /* op start failed */
                            pu_log(LL_ERROR, "%s. Write op finish failed %d %s. Reconnect", PT_THREAD_NAME, errno, strerror(errno));
                            reconnect = 1;
                            break;
                        }
                        pu_log(LL_DEBUG, "%s: sent to WUD %s", PT_THREAD_NAME, out_buf);
                    }
                    break;
                }
                case AQ_Timeout:
/*                          pu_log(LL_WARNING, "%s: timeout on waiting from server queue", PT_THREAD_NAME); */
                    break;
                case AQ_STOP:
                    at_set_stop_wud_write();
                    break;
                default:
                    pu_log(LL_ERROR, "%s: Undefined event %d on wait!", PT_THREAD_NAME, ev);
                    break;
            }
            if (reconnect) {
                lib_tcp_client_close(write_socket);
                pu_log(LL_WARNING, "%s: reconnect");
                break;  /* inner while(!stop) */
            }
        }
        lib_tcp_client_close(write_socket);
    }
    pu_log(LL_INFO, "%s is finished", PT_THREAD_NAME);
    pthread_exit(NULL);
}


/* Start thread */
int at_start_wud_write() {
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &wud_write, NULL)) return 0;
    stop = 0;
    return 1;
}

/* Stop the thread */
void at_stop_wud_write() {
    void *ret;
    at_set_stop_wud_write();

    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}

/* Set stip flag on for async stop */
void at_set_stop_wud_write() {
    stop = 1;
}
