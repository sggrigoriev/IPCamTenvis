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
        ALERT_TO + FILE_TO (AG_DB_STATE_MD_COUNTDOWN)
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
#include "lib_timer.h"

#include "aq_queues.h"
#include "ag_settings.h"

#include "at_cam_alerts_reader.h"

#define AT_THREAD_NAME "CAM_ALERTS_READER"

/******************************************************************
 * Local data
 */
static pthread_t id;
static pthread_attr_t attr;

static volatile int stop=1;                 /* Thread stop flag */

static char out_buf[LIB_HTTP_MAX_MSG_SIZE]; /* buffer for sending data */

static pu_queue_t* to_agent;                /* transport here */

static lib_timer_clock_t alarm_to_io={0};              /* 'Alarm over' clocks */
static lib_timer_clock_t alarm_to_md={0};
static lib_timer_clock_t alarm_to_sd={0};
static lib_timer_clock_t wd_clock = {0};
static int is_md=0, is_sd=0, is_io=0;

/**********************************************************************/
static t_ac_cam_events monitor_wrapper(int to_sec, int alert_to_sec) {
    int ret = em_function(to_sec);

    switch(ret) {
        case EMM_IO_EVENT:
            if(is_io) {
                lib_timer_update(&alarm_to_io);
            }
            else {
                is_io = 1;
                lib_timer_init(&alarm_to_io, alert_to_sec);
                return AC_CAM_START_IO;
            }
            break;
        case EMM_MD_EVENT:
            if(is_md) {
                lib_timer_update(&alarm_to_md);
            }
            else {
                is_md = 1;
                lib_timer_init(&alarm_to_md, alert_to_sec);
                return AC_CAM_START_MD;
            }
            break;
        case EMM_SD_EVENT:
            if(is_sd) {
                lib_timer_update(&alarm_to_sd);
            }
            else {
                is_sd = 1;
                lib_timer_init(&alarm_to_sd, alert_to_sec);
                return AC_CAM_START_SD;
            }
            break;
        case EMM_TIMEOUT:   /* This is our own timeout - time to check alarm clocks */
            if(lib_timer_alarm(alarm_to_md) && (is_md)) {
                is_md = 0;
                return AC_CAM_STOP_MD;
            }
            if(lib_timer_alarm(alarm_to_sd) && (is_sd)) {
                is_sd = 0;
                return AC_CAM_STOP_SD;
            }
            if(lib_timer_alarm(alarm_to_io) && (is_io)) {
                is_io = 0;
                return AC_CAM_STOP_IO;
            }
            if(lib_timer_alarm(wd_clock)) {
                lib_timer_init(&wd_clock, DEFAULT_TO_AR_PING);
                return AC_CAM_TIME_TO_PING;
            }
            break;
        case EMM_CAM_TO:    /* Cam's monitor timeout */
            break;
        case EMM_ALRM_IGNOR:
            pu_log(LL_WARNING, "%s: Disabled alarm came from Camera. Ignored.", __FUNCTION__);
            break;
/* Startup */
        case EMM_LOGIN:
            pu_log(LL_INFO, "%s: Login to Event Monitor Ok.", __FUNCTION__);
            break;
        case EMM_CONN_START:
            pu_log(LL_INFO, "%s: Start connection to Event Monitor Ok.", __FUNCTION__);
            break;
        case EMM_CONN_FINISH:
            pu_log(LL_INFO, "%s: Connected to Event Monitor Ok.", __FUNCTION__);
            break;
/* Error cases */
        case EMM_AB_EVENT:
            pu_log(LL_WARNING, "%s: Abnormal event came from Camera! Restart Monitor!", __FUNCTION__);
            return AC_CAM_STOP_SERVICE;
            break;
        case EMM_READ_ERROR:
            pu_log(LL_WARNING, "%s: Read error event came from Events Monitor! Ignored.", __FUNCTION__);
            break;
        case EMM_NO_RESPOND:
            pu_log(LL_WARNING, "%s: No Respond event came from Camera! Ignored.", __FUNCTION__);
            break;
        case EMM_SELECT_ERR:
            pu_log(LL_ERROR, "%s: Select pipe error in EentsMonitor: %d %s. Restart.", __FUNCTION__, errno, strerror(errno));
            return AC_CAM_STOP_SERVICE;
         case EMM_PEERCLOSED:
            pu_log(LL_ERROR, "%s: Peer Closed alarm came from Camera. Restart.", __FUNCTION__);
            return AC_CAM_STOP_SERVICE;
        default:
            pu_log(LL_ERROR, "%s: Unrecognized event %d from Cam. Restart.", __FUNCTION__, ret);
            return AC_CAM_STOP_SERVICE;
            break;
    }
    return AC_CAM_EVENT_UNDEF;
}
static void* thread_function(void* params) {
    pu_log(LL_INFO, "%s starts", AT_THREAD_NAME);

    stop = 0;
    to_agent = aq_get_gueue(AQ_FromCam);

    time_t md_start=0, sd_start=0, io_start=0;
    lib_timer_init(&wd_clock, DEFAULT_TO_AR_PING);

    while(!stop) {
        t_ac_cam_events ret = monitor_wrapper(DEFAULT_AM_READ_TO_SEC, DEFAULT_AM_ALERT_TO_SEC);
        switch(ret) {
            case AC_CAM_EVENT_UNDEF:    /* nothing interesting - just an intermediate alarm */
                continue;
            case AC_CAM_START_MD:
                md_start = time(NULL);
                ao_make_cam_alert(ret, md_start, 0, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_MD:
                ao_make_cam_alert(ret, md_start, time(NULL), out_buf, sizeof(out_buf));
                 break;
            case AC_CAM_START_SD:
                sd_start = time(NULL);
                ao_make_cam_alert(ret, sd_start, 0, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_SD:
                ao_make_cam_alert(ret, sd_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case AC_CAM_START_IO:
                io_start = time(NULL);
                ao_make_cam_alert(ret, io_start, 0, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_IO:
                ao_make_cam_alert(ret, io_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_SERVICE:
                ao_make_cam_alert(ret, 0, 0, out_buf, sizeof(out_buf));
                stop = 1;
                break;
            case AC_CAM_TIME_TO_PING:
                ao_make_cam_alert(ret, 0, 0, out_buf, sizeof(out_buf));
                break;
            default:
                pu_log(LL_ERROR, "%s: Unrecognized event %d. Ignored", AT_THREAD_NAME, ret);
                break;
        }
        pu_log(LL_DEBUG, "%s: %s event arrived", AT_THREAD_NAME, ac_cam_event2string(ret));

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

/* Set st0p flag on for async stop */
void at_set_stop_cam_alerts_reader() {
    stop = 1;
}