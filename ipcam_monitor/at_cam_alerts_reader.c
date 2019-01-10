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

#include "pu_logger.h"
#include "lib_timer.h"
#include "lib_tcp.h"

#include "ag_defaults.h"
#include "ao_cmd_data.h"
#include "at_cam_files_sender.h"
#include "event_monitor_module.h"
#include "at_cam_alerts_reader.h"

#define AT_THREAD_NAME "CAM_ALERTS_READER"

extern volatile uint32_t contextId;

/******************************************************************
 * Local data
 */
static char out_buf[512]; /* buffer for sending data */

static int to_agent = -1;                /* transport here */
static int from_agent = -1;             /* for SF thread */
static int server_socket = -1;

static lib_timer_clock_t em_restart_to={0};

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
    return sock;
}
/*
 * Return !0 if OK, 0 if not
 */
static int mon_write(int wr_sock, const char* msg) {
    int ret = lib_tcp_write(wr_sock, msg, strlen(msg)+1, 10);    /* 10 seconds timeout */
    if(!ret) {
        pu_log(LL_ERROR, "%s: Timeout writing to Agent.");
    }
    if(ret != strlen(msg)+1) ret = 0;
    return ret;
}
/*
 * If the message starts from "eof: " ther cut off this prefix and return 1.
 * Else do not update msg ane returns 0
 */
static int process_file_message(char* msg, size_t size) {
    size_t msg_len = strlen(msg);
    size_t pref_len = strlen(DEFAULT_MON_FMSG_PREFIX);

    if((msg_len <= pref_len) || strncmp(msg, DEFAULT_MON_FMSG_PREFIX, pref_len) != 0) return 0;
    memmove(msg, msg+pref_len, msg_len-pref_len+1);
    return 1;
}

static t_ac_cam_events monitor_wrapper(int to_sec, int alert_to_sec, char* msg, size_t size) {
    int ret = em_function(to_sec, msg, size);

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
                lib_timer_init(&wd_clock, 60);
                return AC_CAM_TIME_TO_PING;
            }
            break;
        case EMM_GOT_MSG:
            pu_log(LL_INFO, "%s: Got message: %s", __FUNCTION__, msg);
            if(process_file_message(msg, size))
                return AC_CAM_GOT_FILE;
            else
                pu_log(LL_INFO, "%s: Wrong message - ignored", __FUNCTION__, msg);
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
            pu_log(LL_ERROR, "%s: 'Peer Closed' alarm came from Camera. Restart.", __FUNCTION__);
            return AC_CAM_STOP_SERVICE;
        case EMM_CONN_FAILED:
            pu_log(LL_ERROR, "%s: Connection to Cam failed. Restart.", __FUNCTION__);
            return AC_CAM_STOP_SERVICE;
        case EMM_AUTH_FAILED:
            pu_log(LL_ERROR, "%s: Auth Cam failed. Restart.", __FUNCTION__);
            return AC_CAM_STOP_SERVICE;
        case EMM_DISCONNECTED:
            pu_log(LL_ERROR, "%s: Cam disconnected. Restart.", __FUNCTION__);
            return AC_CAM_STOP_SERVICE;
        case EMM_NO_RESPONSE:
            pu_log(LL_ERROR, "%s: No response from Cam. Restart.", __FUNCTION__);
            return AC_CAM_STOP_SERVICE;
        case EMM_LOGOUT:
            pu_log(LL_ERROR, "%s: Cam reported logout. Restart.", __FUNCTION__);
            return AC_CAM_STOP_SERVICE;
        default:
            pu_log(LL_ERROR, "%s: Unrecognized event %d from Cam. Restart.", __FUNCTION__, ret);
            return AC_CAM_STOP_SERVICE;
    }
    return AC_CAM_EVENT_UNDEF;
}

static void monitor() {
    pu_log(LL_INFO, "%s starts", AT_THREAD_NAME);

    time_t md_start=0, sd_start=0, io_start=0;
    lib_timer_init(&wd_clock, 60);
    lib_timer_init(&em_restart_to, 24*3600);

    while(1) {
        char buf[256]={0};
        t_ac_cam_events ret = monitor_wrapper(DEFAULT_AM_READ_TO_SEC, DEFAULT_AM_ALERT_TO_SEC, buf, sizeof(buf));
        switch(ret) {
            case AC_CAM_EVENT_UNDEF:    /* nothing interesting - just an intermediate alarm */
                continue;
            case AC_CAM_START_MD:
                md_start = time(NULL);
                ao_make_MDSD(ret, md_start, 0, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_MD:
                ao_make_MDSD(ret, md_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case AC_CAM_START_SD:
                sd_start = time(NULL);
                ao_make_MDSD(ret, sd_start, 0, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_SD:
                ao_make_MDSD(ret, sd_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case AC_CAM_START_IO:
                io_start = time(NULL);
                ao_make_MDSD(ret, io_start, 0, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_IO:
                ao_make_MDSD(ret, io_start, time(NULL), out_buf, sizeof(out_buf));
                break;
            case AC_CAM_STOP_SERVICE:
                ao_make_MDSD(ret, 0, 0, out_buf, sizeof(out_buf));
                return;
            case AC_CAM_TIME_TO_PING:
                ao_make_MDSD(ret, 0, 0, out_buf, sizeof(out_buf));
                break;
            case AC_CAM_GOT_FILE:
                ao_make_got_files(buf, out_buf, sizeof(out_buf));
                break;
            default:
                pu_log(LL_ERROR, "%s: Unrecognized event %d. Exited.", AT_THREAD_NAME, ret);
                goto on_exit;
        }
        pu_log(LL_DEBUG, "%s: %s event arrived", AT_THREAD_NAME, ac_cam_event2string(ret));

        if(!mon_write(to_agent, out_buf)) {
            pu_log(LL_ERROR, "%s: Can't send msg to Agent. Exiting", AT_THREAD_NAME);
            goto on_exit;
        }
        pu_log(LL_DEBUG, "%s: %s was sent to the Agent", AT_THREAD_NAME, out_buf);

        if(lib_timer_alarm(em_restart_to)) {
            pu_log(LL_INFO, "%s: Time to reboot. Just on case...", AT_THREAD_NAME);
            goto on_exit;
        }
    }
    on_exit:
    pu_log(LL_INFO, "%s stops", AT_THREAD_NAME);
}
/**********************************************************************/
void at_mon_stop() {
    at_stop_sf();
    if(to_agent > 0) lib_tcp_client_close(to_agent);
    if(from_agent > 0) lib_tcp_client_close(from_agent);
    if(server_socket > 0) lib_tcp_client_close(server_socket);
    em_deinit();
}

void at_mon_function(const input_params_t* params) {
    if(to_agent = mon_connect(params->cam_ip, params->cam_port, params->agent_ip, params->agent_port), to_agent < 0) {
        pu_log(LL_ERROR, "%s: Error connection to Agent. Exiting.", AT_THREAD_NAME);
        return;
    }
    if(!em_init(params->cam_ip)) {
        pu_log(LL_ERROR, "%s: Error Cam's Events Monitor setup", AT_THREAD_NAME);
        goto on_exit;
    }
    at_start_sf(dup(to_agent));
    monitor();

    on_exit:
    at_mon_stop();
}
