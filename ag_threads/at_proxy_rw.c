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
#include "pu_logger.h"

#include "ag_settings.h"
#include "at_proxy_read.h"
#include "at_proxy_rw.h"
#include "at_proxy_write.h"


#define AT_THREAD_NAME "AGENT_MAIN"

/*************************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static volatile int stop = 1;       /* Thread stop flag */
static volatile int chids_stop; /* Children (read/write) stop flag */

/* Tread function: creates server socket, listen for Proxy connection; duplicate and have the writable socket; wait until children dead*/
static void* main_thread(void* params) {

    while(!stop) {
        int read_socket = -1;
        int write_socket = -1;
        chids_stop = 0;

        while(read_socket = lib_tcp_get_client_socket(ag_getProxyPort(), 1), read_socket <= 0) {
            pu_log(LL_ERROR, "%s: connection error to Proxy %d %s", AT_THREAD_NAME, errno, strerror(errno));
            sleep(1);
        }
        write_socket = dup(read_socket);
/* Connecting to proxy */
        if(!at_start_proxy_read(read_socket)) {
            pu_log(LL_ERROR, "%s: Creating read thread failed: %s", AT_THREAD_NAME, strerror(errno));
            break;
        }
        pu_log(LL_INFO, "%s: read started.", AT_THREAD_NAME);

        if(!at_start_proxy_write(write_socket)) {
            pu_log(LL_ERROR, "%s: Creating write thread failed: %s", AT_THREAD_NAME, strerror(errno));
            break;
        }
        pu_log(LL_INFO, "%s: write started.", AT_THREAD_NAME);

/* It is hanging on the join inside of stop_agent_read function */
        at_stop_proxy_read();
/* It is hanging on the join inside of stop_agent_write function */
        at_stop_proxy_write();

/* Chilren are dead if we're here */
        pu_log(LL_WARNING, "Agent read/write threads restart");
    }
    pthread_exit(NULL);
}

/*********************************************************************************
 * Public functions
*/

int at_start_proxy_rw() {
    stop = 0;
    if(pthread_attr_init(&attr)) {stop = 1; return 0;}
    if(pthread_create(&id, &attr, &main_thread, NULL)) {stop = 1; return 0;}

    return 1;
}

void at_stop_proxy_rw() {
    void *ret;
    at_set_stop_proxy_rw();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}

void at_set_stop_proxy_rw() {
    stop = 1;
}

void at_set_stop_proxy_rw_children() {
    chids_stop = 1;
}

int at_are_childs_stop() {
    return chids_stop;
}


