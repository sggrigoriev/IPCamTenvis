/*
 *  Copyright 2018 People Power Company
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
 Created by gsg on 18/09/18.
    1. How to determine aler
        ALERT_TO - time between the last alert of given type raised.
        Alert types: MD_ALERT, MD_FILE, SD_ALERT, SD_FILE
        If TO, alert is over,then generate the alert to the app
    2. How to determine the event's file(s) is/are ready?
        ALERT_TO + FILE_TO
        FILE_TO is the time after ALERT_TO is over. - Just to give time to system finish with file writing&saving
    3. How to take all files for the event?
        Event_start_time = the time of the first alert of given type
        Event_stop_time - the time when ALERT_TO raised
        All files foe this time period should be taken!

    AC_CAM_FILE-* message should contain the list of all files needed with full path & name

*/

#include <pthread.h>
#include <string.h>
#include <errno.h>

#include <event_monitor_module.h>

#include "lib_http.h"
#include "pu_logger.h"

#include "aq_queues.h"
#include "ag_settings.h"

#include "at_cam_alerts_reader.h"

#define AT_THREAD_NAME "CAM_ALERTS_READER"



/******************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static volatile int stop=1;                   /* Thread stop flag */

static char out_buf[LIB_HTTP_MAX_MSG_SIZE];                   /* bufffer for sending data */

static pu_queue_t* to_agent;                  /* transport here */

/**********************************************************************/
static send_start(t_ac_cam_events event, time_t start_date, char* buf, size_t size) {

}
static send_stop(t_ac_cam_events event, time_t start_date, time_t end_date, char* buf, size_t size) {

}
static send_event(t_ac_cam_events event, char* buf, size_t size) {

}
static t_ac_cam_events monitor_wrapper(int to_sec, int alert_to_sec) {
    int ret = em_function(1);
    switch(ret) {
        case EMM_IO_EVENT:
            break;
        case EMM_MD_EVENT:
            break;
        case EMM_AB_EVENT:
            break;
        case EMM_SD_EVENT:
            break;
        case EMM_READ_ERROR:
            break;
        case EMM_NO_RESPOND:
            break;
        case EMM_TIMEOUT:
            break;
        case EMM_SELECT_ERR:
            break;
        case EMM_ALRM_IGNOR:
            break;
        case EMM_PEERCLOSED:
            break;
        case EMM_LOGIN:
            break;
        case EMM_CONN_START:
            break;
        case EMM_CONN_FINISH:
            break;
        case EMM_CAM_TO:
            break;
        default:
            break;
    }
    return AC_CAM_EVENT_UNDEF;
}
static void* thread_function(void* params) {
    stop = 0;
    pu_log(LL_INFO, "%s starts", AT_THREAD_NAME);
    time_t md_start=0, sd_start=0, io_start=0;
    while(!stop) {
        t_ac_cam_events ret = monitor_wrapper(DEFAULT_AM_READ_TO_SEC, DEFAULT_AM_ALERT_TO_SEC);
        switch(ret) {
            case AC_CAM_START_MD:
                md_start = time(NULL);
                send_start(ret, md_start, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_MD:
                send_stop(ret, md_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case AC_CAM_START_SD:
                sd_start = time(NULL);
                send_start(ret, sd_start, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_SD:
                send_stop(ret, sd_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case AC_CAM_START_IO:
                io_start = time(NULL);
                send_start(ret, io_start, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_IO:
                send_stop(ret, time(NULL), out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_SERVICE:
                send_event(ret, out_buf, sizeof(out_buf));
                stop = 1;
                break;
            default:
                pu_log(LL_ERROR, "%s: Unrecognized event %d. Ignored", AT_THREAD_NAME, ret);
                break;
        }
        pu_log(LL_DEBUG, "%s: %s event arrived", __FUNCTION__, ac_cam_evens2string(ret));

        pu_queue_push(to_agent, out_buf, strlen(out_buf) + 1);
        pu_log(LL_DEBUG, "%s: %s was sent to the Agent", AT_THREAD_NAME, out_buf);
    }
    pu_log(LL_INFO, "%s stops", AT_THREAD_NAME);
    return NULL;
}
/**********************************************************************/

/* Start thread */
int at_start_cam_alerts_reader() {
    if(!stop) {
        pu_log(LL_ERROR, "%s already started", AT_THREAD_NAME);
        return 1;
    }
    if(!em_init(ag_getCamIP())) {
        pu_log(LL_ERROR, "%s: Eror Cam's Events Monitor setup", AT_THREAD_NAME);
        return 0;
    }

    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &thread_function, NULL)) return 0;

    return 1;
}

/* Stop the thread */
void at_stop_cam_alerts_reader() {
    void *ret;

    if(stop) {
        pu_log(LL_ERROR, "%s: %s already stops", __FUNCTION__, AT_THREAD_NAME);
        return;
    }
    stop = 1;
    em_deinit();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}

/* Set stip flag on for async stop */
void at_set_stop_cam_alerts_reader() {
    stop = 1;
}