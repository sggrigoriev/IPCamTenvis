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

#include "lib_http.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "at_cam_control.h"

#define PT_THREAD_NAME "CAM_CONTROL"

/************************************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static volatile int stop;       /* Thread stop flag */

static pu_queue_t* from_cam_control;    /* cam_control -> main_thread */
static pu_queue_t* to_cam_control;      /* main_thread -> cam_control */
static pu_queue_t* from_camera;         /* camera -> cam_control */
static pu_queue_t* to_camera;           /* cam_control -> camera */

static pu_queue_msg_t mt_msg[LIB_HTTP_MAX_MSG_SIZE];    /* The only main thread's buffer! */
/*****************************************************************************************
 * Local functions
 */
static void* cam_control(void* params);

/**********************************************
 * Public functione definition
 */
void at_start_cam_control() {

}

void at_stop_cam_control() {

}

void at_set_stop_cam_control() {

}
/***********************************************************/
static void* cam_control(void* params) {

}