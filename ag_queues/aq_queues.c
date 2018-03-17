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
 Created by gsg on 18/10/17.
*/

#include "ag_settings.h"
#include "aq_queues.h"

/***************************************************************************
 * Local data
 */
static pu_queue_t* qu_arr[AQ_MAX_QUEUE-AQ_MIN_QUEUE+1];     /* Proxy queues pool */
static pthread_mutex_t  own_mutex;                          /* Pool concurrent use protection */

/******************************************************************************
 * Public functions implementation
*/
void aq_init_queues() {
    pthread_mutex_lock(&own_mutex);
    pu_queues_init(AQ_MAX_QUEUE-AQ_MIN_QUEUE+1);
    unsigned int i;
    for(i = 0; i < AQ_MAX_QUEUE-AQ_MIN_QUEUE+1; i++) {
        qu_arr[i] = pu_queue_create(ag_getQueuesRecAmt(),i+AQ_MIN_QUEUE);
    }
    pthread_mutex_unlock(&own_mutex);
}

void aq_erase_queues() {
    pthread_mutex_lock(&own_mutex);
    int i;
    for(i = 0; i < AQ_MAX_QUEUE-AQ_MIN_QUEUE+1; i++) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waggressive-loop-optimizations"
        pu_queue_erase(qu_arr[i+AQ_MIN_QUEUE]);             /* There are no arrays like [1..4] in C ... */
#pragma GCC diagnostic pop
    }
    pu_queues_destroy();
    pthread_mutex_unlock(&own_mutex);
}

pu_queue_t* aq_get_gueue(int que_number) {
    if(que_number < AQ_MIN_QUEUE) return NULL;
    if(que_number >AQ_MAX_QUEUE) return NULL;

    pthread_mutex_lock(&own_mutex);
    pu_queue_t* ret = qu_arr[que_number-AQ_MIN_QUEUE];
    pthread_mutex_unlock(&own_mutex);
    return ret;
}

queue_events_t ag_get_non_empty_queue() {
    queue_events_t i;
    for(i = AQ_MIN_QUEUE; i <= AQ_MAX_QUEUE;  i++) {
        if(!pu_queue_empty(aq_get_gueue(i))) return i;
    }
    return AQ_Timeout;
}