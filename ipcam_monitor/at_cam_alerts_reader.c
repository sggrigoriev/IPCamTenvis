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
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <lib_tcp.h>

#include "pu_logger.h"
#include "lib_timer.h"

#include "event_monitor_module.h"
#include "at_cam_alerts_reader.h"

#define AT_THREAD_NAME "CAM_ALERTS_READER"
#define IP_CTX_(a) contextId = a

extern volatile uint32_t contextId;

/******************************************************************
 * Local data
 */
static char out_buf[512]; /* buffer for sending data */

static int to_agent = -1;                /* transport here */
static int server_socket = -1;

static lib_timer_clock_t alarm_to_io={0};              /* 'Alarm over' clocks */
static lib_timer_clock_t alarm_to_md={0};
static lib_timer_clock_t alarm_to_sd={0};
static lib_timer_clock_t wd_clock = {0};
static int is_md=0, is_sd=0, is_io=0;

/************************Transport functions**********************************************/
/*
 * Return write socket to agent or -1 if error
 */
static int mon_connect(const char* cam_ip, int cam_port, const char* agent_ip, int agent_port) {
    IP_CTX_(10000);
    server_socket = lib_tcp_get_server_socket(agent_port);
    if(server_socket < 0) {
        pu_log(LL_ERROR, "%s: unable to bind to port %d. %d %s", __FUNCTION__, agent_port, errno, strerror(errno));
        return -1;
    }
    int sock = lib_tcp_listen(server_socket, 60);  /* 60 seconds timeout */
    if(sock < 0) {
        pu_log(LL_ERROR, "%s: listen error.", AT_THREAD_NAME);
        close(server_socket);
        return -1;
    }
    if(!sock) {
        pu_log(LL_ERROR, "%s: Timeout on connection attempt!", __FUNCTION__);
        close(server_socket);
        return -1;
    }
    pu_log(LL_INFO, "%s: connected to Agent by socket %d", __FUNCTION__, sock);
    IP_CTX_(10001);
    return sock;
}
/*
 * Return !0 if OK, 0 if not
 */
static int mon_write(int wr_sock, const char* msg) {
    IP_CTX_(20000);
    int ret = lib_tcp_write(wr_sock, msg, strlen(msg)+1, 10);    /* 10 seconds timeout */
    if(!ret) {
        pu_log(LL_ERROR, "%s: Timeout writing to Agent.");
    }
    if(ret != strlen(msg)+1) ret = 0;
    return ret;
}

/**********************************************************************/
/* TODO: make common lin with the Agent! */
/* Alert thread constants */
#define ALERT_NAME   "alertName"
#define ALERT_START  "startDate"
#define ALERT_END    "endDate"

typedef enum {
    CAM_EVENT_UNDEF,
    CAM_START_MD, CAM_STOP_MD, CAM_START_SD, CAM_STOP_SD, CAM_START_IO, CAM_STOP_IO,
    CAM_MADE_SNAPSHOT, CAM_RECORD_VIDEO,
    CAM_STOP_SERVICE, CAM_TIME_TO_PING,
    CAM_EVENTS_SIZE
} cam_events_t;             /* NB! Copypasted from ao_cmd_data.h */
static const char* CAM_EVENTS_NAMES[CAM_EVENTS_SIZE] = {
        "AC_CAM_EVENT_UNDEF",
        "AC_CAM_START_MD", "AC_CAM_STOP_MD", "AC_CAM_START_SD", "AC_CAM_STOP_SD", "AC_CAM_START_IO", "AC_CAM_STOP_IO",
        "AC_CAM_MADE_SNAPSHOT", "AC_CAM_RECORD_VIDEO",
        "AC_CAM_STOP_SERVICE", "AC_CAM_TIME_TO_PING"
};
static const char* cam_event2string(cam_events_t e) {
    IP_CTX_(30000);
    return ((e < CAM_EVENT_UNDEF) || (e >= CAM_EVENTS_SIZE))?
           "Unknown event" :
           CAM_EVENTS_NAMES[e];
}
/*
 * Notify Agent about the event
 * if start_date or end_date is 0 fields are ignored
 * {"alertName" : "AC_CAM_STOP_MD", "startDate" : 1537627300, "endDate" : 1537627488}
 */
const char* make_cam_alert(cam_events_t event, time_t start_date, time_t end_date, char* buf, size_t size) {
    IP_CTX_(40000);
    const char* alert = "{\""ALERT_NAME"\" : \"%s\"}";
    const char* alert_start = "{\""ALERT_NAME"\" : \"%s\", \""ALERT_START"\" : %lu}";
    const char* alert_stop = "{\""ALERT_NAME"\" : \"%s\", \""ALERT_START"\" : %lu, \""ALERT_END"\" : %lu}";

    if(!start_date)
        snprintf(buf, size-1, alert, cam_event2string(event));
    else if(!end_date)
        snprintf(buf, size-1, alert_start, cam_event2string(event), start_date);
    else
        snprintf(buf, size-1, alert_stop, cam_event2string(event), start_date, end_date);

    buf[size] = '\0';
    IP_CTX_(40000);
    return buf;
}
static cam_events_t monitor_wrapper(int to_sec, int alert_to_sec) {
    IP_CTX_(50000);
    int ret = em_function(to_sec);

    switch(ret) {
        case EMM_IO_EVENT:
            if(is_io) {
                lib_timer_update(&alarm_to_io);
            }
            else {
                is_io = 1;
                lib_timer_init(&alarm_to_io, alert_to_sec);
                return CAM_START_IO;
            }
            break;
        case EMM_MD_EVENT:
            if(is_md) {
                lib_timer_update(&alarm_to_md);
            }
            else {
                is_md = 1;
                lib_timer_init(&alarm_to_md, alert_to_sec);
                return CAM_START_MD;
            }
            break;
        case EMM_SD_EVENT:
            if(is_sd) {
                lib_timer_update(&alarm_to_sd);
            }
            else {
                is_sd = 1;
                lib_timer_init(&alarm_to_sd, alert_to_sec);
                return CAM_START_SD;
            }
            break;
        case EMM_TIMEOUT:   /* This is our own timeout - time to check alarm clocks */
            if(lib_timer_alarm(alarm_to_md) && (is_md)) {
                is_md = 0;
                return CAM_STOP_MD;
            }
            if(lib_timer_alarm(alarm_to_sd) && (is_sd)) {
                is_sd = 0;
                return CAM_STOP_SD;
            }
            if(lib_timer_alarm(alarm_to_io) && (is_io)) {
                is_io = 0;
                return CAM_STOP_IO;
            }
            if(lib_timer_alarm(wd_clock)) {
                lib_timer_init(&wd_clock, 60);
                return CAM_TIME_TO_PING;
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
            return CAM_STOP_SERVICE;
            break;
        case EMM_READ_ERROR:
            pu_log(LL_WARNING, "%s: Read error event came from Events Monitor! Ignored.", __FUNCTION__);
            break;
        case EMM_NO_RESPOND:
            pu_log(LL_WARNING, "%s: No Respond event came from Camera! Ignored.", __FUNCTION__);
            break;
        case EMM_SELECT_ERR:
            pu_log(LL_ERROR, "%s: Select pipe error in EentsMonitor: %d %s. Restart.", __FUNCTION__, errno, strerror(errno));
            return CAM_STOP_SERVICE;
         case EMM_PEERCLOSED:
            pu_log(LL_ERROR, "%s: 'Peer Closed' alarm came from Camera. Restart.", __FUNCTION__);
            return CAM_STOP_SERVICE;
        case EMM_CONN_FAILED:
            pu_log(LL_ERROR, "%s: Connection to Cam failed. Restart.", __FUNCTION__);
            return CAM_STOP_SERVICE;
        case EMM_AUTH_FAILED:
            pu_log(LL_ERROR, "%s: Auth Cam failed. Restart.", __FUNCTION__);
            return CAM_STOP_SERVICE;
        case EMM_DISCONNECTED:
            pu_log(LL_ERROR, "%s: Cam disconnected. Restart.", __FUNCTION__);
            return CAM_STOP_SERVICE;
        case EMM_NO_RESPONSE:
            pu_log(LL_ERROR, "%s: No response from Cam. Restart.", __FUNCTION__);
            return CAM_STOP_SERVICE;
        case EMM_LOGOUT:
            pu_log(LL_ERROR, "%s: Cam reported logout. Restart.", __FUNCTION__);
            return CAM_STOP_SERVICE;
        default:
            pu_log(LL_ERROR, "%s: Unrecognized event %d from Cam. Restart.", __FUNCTION__, ret);
            return CAM_STOP_SERVICE;
    }
    IP_CTX_(50001);
    return CAM_EVENT_UNDEF;
}

static void monitor() {
    IP_CTX_(60000);
    pu_log(LL_INFO, "%s starts", AT_THREAD_NAME);

    time_t md_start=0, sd_start=0, io_start=0;
    lib_timer_init(&wd_clock, 60);

    while(1) {
        cam_events_t ret = monitor_wrapper(/*DEFAULT_AM_READ_TO_SEC*/1, /*DEFAULT_AM_ALERT_TO_SEC*/10);
        switch(ret) {
            case CAM_EVENT_UNDEF:    /* nothing interesting - just an intermediate alarm */
                continue;
            case CAM_START_MD:
                md_start = time(NULL);
                make_cam_alert(ret, md_start, 0, out_buf, sizeof(out_buf));
                break;
            case CAM_STOP_MD:
                make_cam_alert(ret, md_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case CAM_START_SD:
                sd_start = time(NULL);
                make_cam_alert(ret, sd_start, 0, out_buf, sizeof(out_buf));
                break;
            case CAM_STOP_SD:
                make_cam_alert(ret, sd_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case CAM_START_IO:
                io_start = time(NULL);
                make_cam_alert(ret, io_start, 0, out_buf, sizeof(out_buf));
                break;
            case CAM_STOP_IO:
                make_cam_alert(ret, io_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case CAM_STOP_SERVICE:
                make_cam_alert(ret, 0, 0, out_buf, sizeof(out_buf));
                return;
             case CAM_TIME_TO_PING:
                make_cam_alert(ret, 0, 0, out_buf, sizeof(out_buf));
                break;
            default:
                pu_log(LL_ERROR, "%s: Unrecognized event %d. Exited.", AT_THREAD_NAME, ret);
                goto on_exit;
        }
        pu_log(LL_DEBUG, "%s: %s event arrived", AT_THREAD_NAME, cam_event2string(ret));

        if(!mon_write(to_agent, out_buf)) {
            pu_log(LL_ERROR, "%s: Can't send msg to Agent. Exiting", AT_THREAD_NAME);
            goto on_exit;
        }
        pu_log(LL_DEBUG, "%s: %s was sent to the Agent", AT_THREAD_NAME, out_buf);
    }
on_exit:
    IP_CTX_(60001);
    pu_log(LL_INFO, "%s stops", AT_THREAD_NAME);
}
/**********************************************************************/
void at_mon_stop() {
    IP_CTX_(70000);
    if(to_agent > 0) lib_tcp_client_close(to_agent);
    if(server_socket > 0) lib_tcp_client_close(server_socket);
    IP_CTX_(70300);
    em_deinit();
    IP_CTX_(70001);
}

void at_mon_function(const input_params_t* params) {
    IP_CTX_(80000);
    if(to_agent = mon_connect(params->cam_ip, params->cam_port, params->agent_ip, params->agent_port), to_agent < 0) {
        pu_log(LL_ERROR, "%s: Error connection to Agent. Exiting.", AT_THREAD_NAME);
        return;
    }
    if(!em_init(params->cam_ip)) {
        pu_log(LL_ERROR, "%s: Error Cam's Events Monitor setup", AT_THREAD_NAME);
        goto on_exit;
    }
    monitor();

on_exit:
    IP_CTX_(80001);
    at_mon_stop();
}



