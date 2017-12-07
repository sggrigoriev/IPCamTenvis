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
 Localization for Agent's queues
*/

#ifndef IPCAMTENVIS_AQ_QUEUES_H
#define IPCAMTENVIS_AQ_QUEUES_H

#include "pu_queue.h"

#define AQ_MIN_QUEUE AQ_FromProxyQueue  /* The event with min number. Needed because of default STOP event */
#define AQ_MAX_QUEUE AQ_ToVideoMgr      /* Event with the max user event number - las one before AQ_Stop */


/* Full Agent's queues events list */
typedef enum {AQ_Timeout = PQ_TIMEOUT,
    AQ_FromProxyQueue = 1, AQ_ToProxyQueue = 2, AQ_FromCamControl = 3, AQ_ToCamControl = 4, AQ_FromWS = 5, AQ_ToVideoMgr = 6,
    AQ_STOP = PQ_STOP} queue_events_t;

/* Init Agent queues service */
void aq_init_queues();

/* Stop Agent queues service */
void aq_erase_queues();

/* Get the queue pointed by associated event number
 *  Return pointer to the queue or NULL if no queue associated
 */
pu_queue_t* aq_get_gueue(int que_number);

#endif /* IPCAMTENVIS_AQ_QUEUES_H */
