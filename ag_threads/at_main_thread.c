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

#include <memory.h>
#include <errno.h>

#include "lib_timer.h"
#include "pu_queue.h"
#include "pu_logger.h"
#include "pr_ptr_list.h"

#include "aq_queues.h"
#include "ag_settings.h"

#include "at_proxy_rw.h"
#include "at_wud_write.h"
#include "at_cam_alerts_reader.h"
#include "at_ws.h"
#include "at_cam_files_sender.h"

#include "ao_cmd_data.h"
#include "ao_cmd_proxy.h"
#include "pr_commands.h"
#include "ac_video.h"

#include "ac_cam.h"
#include "ao_cmd_cloud.h"
#include "ao_cmd_data.h"
#include "ag_db_mgr.h"
#include "au_string.h"

#include "at_main_thread.h"

#define AT_THREAD_NAME  "IPCamTenvis"

/****************************************************************************************
    Main thread global variables
*/

extern uint32_t contextId;

static pu_queue_msg_t mt_msg[LIB_HTTP_MAX_MSG_SIZE];    /* The only main thread's buffer! */
static pu_queue_event_t events;     /* main thread events set */
static pu_queue_t* from_poxy;       /* proxy_read -> main_thread */
static pu_queue_t* to_proxy;        /* main_thread->proxy */
static pu_queue_t* to_wud;          /* main_thread -> wud_write  */
static pu_queue_t* from_cam;        /* cam -> main_thread */
static pu_queue_t* from_ws;         /* WS -> main_thread */
static pu_queue_t* from_stream_rw;  /* from streaming threads to main */
static pu_queue_t* to_sf;           /* from main_thread to files sender */

static volatile int main_finish;        /* stop flag for main thread */

/*****************************************************************************************
    Local functions and types definitions
*/
typedef enum {
    MT_PROTO_UNDEF,     /* ERROR */
    MT_PROTO_OWN,     /* own "gw_cloudConnection" (from Proxy), "filesSent" (from WUD)*/
    MT_PROTO_CLOUD,     /* commands from cloud */
    MT_PROTO_WS,        /* from WebSocket */
    MT_PROTO_EM,    /* from events monitor */
    MT_PROTO_STREAMING  /* from streamer */
} protocol_type_t;
/*
 * To Proxy & WUD
 */
static void send_startup_report() {
    IP_CTX_(1000);
    pu_log(LL_DEBUG, "%s: Startup report preparing", __FUNCTION__);
    cJSON* report = ag_db_get_startup_report();
    if(report) {
        char buf[LIB_HTTP_MAX_MSG_SIZE];
        const char* msg = ao_cloud_msg(ag_getProxyID(), "153", NULL, NULL, ao_cloud_measures(report, ag_getProxyID()), buf, sizeof(buf));
        cJSON_Delete(report);
        if(!msg) {
            pu_log(LL_ERROR, "%s: message to cloud exceeds max size %d. Ignored", __FUNCTION__, LIB_HTTP_MAX_MSG_SIZE);
            IP_CTX_(1001);
            return;
        }
        pu_queue_push(to_proxy, msg, strlen(msg)+1);
    }
    else {
        pu_log(LL_ERROR, "%s: Error startup report creation. Nothing was sent.", __FUNCTION__);
    }
    IP_CTX_(1002);
}
static void send_wd() {
    IP_CTX_(2000);
    char buf[LIB_HTTP_MAX_MSG_SIZE];

    pr_make_wd_alert4WUD(buf, sizeof(buf), ag_getAgentName(), ag_getProxyID());
    pu_queue_push(to_wud, buf, strlen(buf)+1);
    IP_CTX_(2001);
}
static void send_reboot() {
    IP_CTX_(3000);
    char buf[LIB_HTTP_MAX_MSG_SIZE] = {0};

    pu_log(LL_INFO, "%s: Cam Agent requests for reboot", __FUNCTION__);
    pr_make_reboot_command(buf, sizeof(buf), ag_getProxyID());
    pu_queue_push(to_wud, buf, strlen(buf) + 1);
    IP_CTX_(3001);
}
static void send_ACK_to_Proxy(int command_number) {
    IP_CTX_(4000);
    char buf[128];
    ao_cloud_msg(ag_getProxyID(), "154", NULL, ao_cloud_responses(command_number, 0), NULL, buf, sizeof(buf));
    pu_queue_push(to_proxy, buf, strlen(buf) + 1);
    IP_CTX_(4001);
}


static void send_snapshot(const char* full_path) {
    IP_CTX_(5000);
    char buf[LIB_HTTP_MAX_MSG_SIZE];
    char path[1024]={0};
    snprintf(path, sizeof(path)-1, "\"%s\"", full_path);
    ao_make_send_files(buf, sizeof(buf), 0, ac_get_event2file_type(AC_CAM_MADE_SNAPSHOT), path);
    pu_log(LL_DEBUG, "%s: Sending to SF thread: %s", __FUNCTION__, buf);
    pu_queue_push(to_sf, buf, strlen(buf) + 1);
    IP_CTX_(5001);
}
static void send_send_file(t_ao_cam_alert data) {
    IP_CTX_(6000);
    char buf[LIB_HTTP_MAX_MSG_SIZE];
    char* f_list = ac_cam_get_files_name(ac_get_event2file_type(data.cam_event), data.start_date, data.end_date);

    if(!f_list) {
        pu_log(LL_WARNING, "%s: no alarm files where found - no data set to SF", AT_THREAD_NAME);
    }
    else {
        ao_make_send_files(buf, sizeof(buf), data.end_date, ac_get_event2file_type(data.cam_event), f_list);
        pu_queue_push(to_sf, buf, strlen(buf) + 1);
        free(f_list);
    }
    IP_CTX_(6001);
}
/*
 * Send SD/MD/SNAPHOT files which wasn't sent before
 * Call on startup
 */
static void send_remaining_files() {
    IP_CTX_(7000);
    char buf[LIB_HTTP_MAX_MSG_SIZE];
    char* md;
    char* sd;
    char* snap;
    ac_get_all_files(&md, &sd, &snap);
    if(md) {
        ao_make_send_files(buf, sizeof(buf), 0, DEFAULT_MD_FILE_POSTFIX, md);
        pu_queue_push(to_sf, buf, strlen(buf) + 1);
        pu_log(LL_DEBUG, "%s: remaining MD files %s sent to SF_thread", __FUNCTION__, md);
        free(md);
    }
    if(sd) {
        ao_make_send_files(buf, sizeof(buf), 0, DEFAULT_SD_FILE_POSTFIX, sd);
        pu_queue_push(to_sf, buf, strlen(buf) + 1);
        pu_log(LL_DEBUG, "%s: remaining files %s sent to SF_thread", __FUNCTION__, sd);
        free(sd);
    }
    if(snap) {
        ao_make_send_files(buf, sizeof(buf), 0, DEFAULT_SNAP_FILE_POSTFIX, snap);
        pu_queue_push(to_sf, buf, strlen(buf) + 1);
        pu_log(LL_DEBUG, "%s: remaining SNAP files %s sent to SF_thread", __FUNCTION__, snap);
        free(snap);
    }
    IP_CTX_(7001);
}
/*
 * To WS
 */
static int send_to_ws(const char* msg) {
    IP_CTX_(8000);
    if(!at_ws_send(msg)) {
        pu_log(LL_ERROR, "%s: Error sending  %s to WS. Restart WS required", __FUNCTION__, msg);
        IP_CTX_(8002);
        return 0;
    }
    IP_CTX_(8003);
    return 1;
}

static int send_active_viewers_reqiest(){
    IP_CTX_(10000);
    char sess[128] = {0};
    char buf[256] = {0};
    IP_CTX_(10001);
    return send_to_ws(
            ao_active_viewers_request(ac_get_session_id(sess, sizeof(sess)-1), buf, sizeof(buf)-1)
            );
}
static int send_stream_error() {
    IP_CTX_(11000);
    char buf[512] = {0};
    char err[128] = {0};
    char sess[128] = {0};

    int ret = send_to_ws(
            ao_stream_error_report(ac_get_stream_error(err, sizeof(err)-1),
                                   ac_get_session_id(sess, sizeof(sess)-1),
                                   buf, sizeof(buf)-1)
    );
    ac_clear_stream_error();
    IP_CTX_(11001);
    return ret;
}
static void stop_ws() {
    IP_CTX_(27000);
    ac_stop_video();
    ac_disconnect_video();
    ag_db_set_int_property(AG_DB_STATE_WS_ON, 0);
    IP_CTX_(27001);
}
static void restart_events_monitor() {
    IP_CTX_(22000);
    at_stop_cam_alerts_reader();
    if(!at_start_cam_alerts_reader()) {
        pu_log(LL_ERROR, "%s: Can't restart Events Monitor. Reboot.", __FUNCTION__);
        send_reboot();
    }
    IP_CTX_(22001);
}
/*
 * Agent actions: agent->WS->streaming
 * snapshot
 * cam parameters change + change report 2 WS
 */
static void make_snapshot() {
    IP_CTX_(16000);
    int snapshot_command = ag_db_get_int_property(AG_DB_STATE_SNAPSHOT);
    if (snapshot_command < 1) return;

    char name[30]={0};
    char path[256]={0};

    ac_make_name_from_date(DEFAULLT_SNAP_FILE_PREFIX, time(NULL), DEFAULT_SNAP_FILE_POSTFIX, DEFAULT_SNAP_FILE_EXT, name, sizeof(name)-1);
    snprintf(path, sizeof(path)-1, "%s/%s/%s", DEFAULT_DT_FILES_PATH, DEFAULT_SNAP_DIR, name);
    if (!ac_cam_make_snapshot(path)) {
        pu_log(LL_ERROR, "%s: Error picture creation", __FUNCTION__);
        IP_CTX_(16001);
        return;
    }
    send_snapshot(path);
    ag_db_set_int_property(AG_DB_STATE_SNAPSHOT, 0);
    IP_CTX_(16002);
}
static void run_agent_actions() {
    IP_CTX_(12000);
    ag_db_bin_state_t variant = ag_db_bin_anal(AG_DB_STATE_AGENT_ON);
    switch(variant) {
        case AG_DB_BIN_NO_CHANGE:   /* nothing to do */
        case AG_DB_BIN_OFF_OFF:     /* nothing to do */
        case AG_DB_BIN_ON_OFF:      /* nothing to do */
            break;
        case AG_DB_BIN_OFF_ON:      /* Was disconneced, now connected */
        case AG_DB_BIN_ON_ON:       /* connection info changed - total reconnect! - same as OFF->ON */
            pu_log(LL_INFO, "%s: Got connection info. Connect WS requested", __FUNCTION__);
            send_startup_report();                                  /* send cam's initial settings to the cloud */
            break;
        default:
            pu_log(LL_WARNING, "%s: Unprocessed variant %d", __FUNCTION__, variant);
            break;
    }

    if(ag_db_get_int_property(AG_DB_STATE_AGENT_ON)) {
        if (ag_db_get_int_property(AG_DB_CMD_SEND_WD_AGENT)) {  /* PING processing */
            send_wd();
            ag_db_set_int_property(AG_DB_CMD_SEND_WD_AGENT, 0);
        }
        if(!ag_db_get_int_property(AG_DB_STATE_WS_ON)) {        /* Kick the WS to start if it is not started! */
            ag_db_set_int_property(AG_DB_STATE_WS_ON, 1);
        }
    }
    IP_CTX_(12001);
}
static void run_ws_actions() {
    IP_CTX_(13000);
    ag_db_bin_state_t variant = ag_db_bin_anal(AG_DB_STATE_WS_ON);
    switch(variant) {
        case AG_DB_BIN_NO_CHANGE:   /* nothing to do */
        case AG_DB_BIN_OFF_OFF:     /* nothing to do */
            break;
        case AG_DB_BIN_ON_OFF:      /* Was connected - disconnect required */
            pu_log(LL_DEBUG, "%s: 1->0: Disconnect required", __FUNCTION__);
            stop_ws();
            break;
        case AG_DB_BIN_OFF_ON:      /* Was disconneced, now connected */
            pu_log(LL_DEBUG, "%s: 0->1: Connect required", __FUNCTION__);
            if (!ac_connect_video()) {
                pu_log(LL_ERROR, "%s: Fail to WS interface start. WS inactive.", __FUNCTION__);
                ag_db_set_int_property(AG_DB_STATE_WS_ON, 0);
            }
            else {
                ag_db_set_int_property(AG_DB_STATE_VIEWERS_COUNT, 1);   /* To warm-up streaming unconditionally*/
            }
            break;
        case AG_DB_BIN_ON_ON:       /* 1->1: reconnection request */
            pu_log(LL_DEBUG, "%s: 1->1: Reconnect required", __FUNCTION__);
            stop_ws();
            if (!ac_connect_video()) {
                pu_log(LL_ERROR, "%s: Fail to WS interface start. WS inactive.", __FUNCTION__);
                ag_db_set_int_property(AG_DB_STATE_WS_ON, 0);
            }
            else {
                ag_db_set_int_property(AG_DB_STATE_VIEWERS_COUNT, 1);   /* To warm-up streaming unconditionally*/
            }
            break;
        default:
            pu_log(LL_WARNING, "%s: Unprocessed variant %d", __FUNCTION__, variant);
            break;
    }
    if(ag_db_get_int_property(AG_DB_STATE_WS_ON)) { /* Now actions if connected */
        if (ag_db_get_int_property(AG_DB_CMD_ASK_4_VIEWERS_WS)) {    /* Ask cloud for viewers total amount */
            if(!send_active_viewers_reqiest())
                stop_ws();
            else
                ag_db_set_int_property(AG_DB_CMD_ASK_4_VIEWERS_WS, 0);
        }
        if (ag_db_get_int_property(AG_DB_CMD_PONG_REQUEST)) {        /* Answer by pong to WS's ping */
            if(!send_to_ws(ao_answer_to_ws_ping()))
                stop_ws();
            else
                ag_db_set_int_property(AG_DB_CMD_PONG_REQUEST, 0);
        }
        if (ac_is_stream_error()) {
            if(!send_stream_error()) stop_ws();
        }
    }
    else {  /* And actions if disconnected */
        if(ag_db_get_int_property(AG_DB_STATE_RW_ON)) {
            ag_db_set_int_property(AG_DB_STATE_RW_ON, 0);   /* Ask for streaming disconnect */
        }
    }
    IP_CTX_(13001);
}
static void switch_streaming_on() {
    IP_CTX_(28000);
    if (!ac_start_video(ag_db_get_int_property(AG_DB_STATE_VIDEO), ag_db_get_int_property(AG_DB_STATE_AUDIO))) {
        pu_log(LL_ERROR, "%s: Error RW start. RW inactive.", __FUNCTION__);
        ag_db_set_int_property(AG_DB_STATE_RW_ON, 0);
    }
    else {
        pu_log(LL_DEBUG, "%s: Start streaming", __FUNCTION__);
        ag_db_set_int_property(AG_DB_STATE_RW_ON, 1);
    }
    IP_CTX_(28001);
}
/* on/off audio/video */
static void switch_control_streams() {
    IP_CTX_(29000);
    ag_db_bin_state_t video_state = ag_db_bin_anal(AG_DB_STATE_VIDEO);
    ag_db_bin_state_t audio_state = ag_db_bin_anal(AG_DB_STATE_AUDIO);

    if((video_state == AG_DB_BIN_OFF_ON) || (video_state == AG_DB_BIN_ON_OFF) ||
        (audio_state == AG_DB_BIN_OFF_ON) || (audio_state == AG_DB_BIN_ON_OFF)) {
        pu_log(LL_DEBUG, "%s: Streams change: video = %d, Audio = %d", __FUNCTION__, ag_db_get_int_property(AG_DB_STATE_VIDEO), ag_db_get_int_property(AG_DB_STATE_AUDIO));
        ac_stop_video();
        switch_streaming_on();
    }
    IP_CTX_(29001);
}
static void run_streaming_actions() {
    IP_CTX_(14000);
    ag_db_bin_state_t variant = ag_db_bin_anal(AG_DB_STATE_RW_ON);
    switch (variant) {
        case AG_DB_BIN_NO_CHANGE:    /* no changes */
        case AG_DB_BIN_OFF_OFF:      /* 0->0*/
        case AG_DB_BIN_ON_ON:         /* 1->1 */
            break;
        case AG_DB_BIN_OFF_ON:       /* 0->1*/
            pu_log(LL_INFO, "%s: 0->1 - connection case!", __FUNCTION__);
            switch_streaming_on();
            break;
        case AG_DB_BIN_ON_OFF:       /* 1->0 */
            pu_log(LL_DEBUG, "%s: 1->0 - Stop video", __FUNCTION__);
            ac_stop_video();
            break;
        default:
            pu_log(LL_WARNING, "%s: Unprocessed variant %d", __FUNCTION__, variant);
            break;
    }
    if (ag_db_get_int_property(AG_DB_STATE_RW_ON)) { /* Actions for active streaming */
        switch_control_streams();
        if (!ag_db_get_int_property(AG_DB_STATE_VIEWERS_COUNT)) { /* No active viwers - stop show */
            pu_log(LL_DEBUG, "%s: Stop streaming because of zero active viewers.", __FUNCTION__);
            ac_stop_video();
            ag_db_set_int_property(AG_DB_STATE_RW_ON, 0);
        }
    } else
        {      /* Actions for inactive streaming */
        if(ag_db_get_int_property(AG_DB_STATE_WS_ON) && ag_db_get_int_property(AG_DB_STATE_VIEWERS_COUNT)) { /* Got viewers - start show */
            switch_streaming_on();
        }
        switch(ag_db_bin_anal(AG_DB_STATE_STREAM_STATUS)) {
            case AG_DB_BIN_ON_ON:
            case AG_DB_BIN_OFF_ON:
                switch_streaming_on();
                ag_db_set_int_property(AG_DB_STATE_VIEWERS_COUNT, 1);   /* To warm-up streaming unconditionally*/
                break;
            default:
                break;
        }
    }
    IP_CTX_(14001);
}
static void run_cam_actions() {
/* Switch On/Off MD */
/* Switch On/Off SD */
/* Change Audio sensitivity */
/* Change Video sensitivity */
/* Make snapshit */
    IP_CTX_(15000);
    ag_db_update_changed_cam_parameters();
    make_snapshot();
    IP_CTX_(15001);
}
static void send_reports() {
    IP_CTX_(30000);
/* Make changes report */
    char buf[LIB_HTTP_MAX_MSG_SIZE];
    cJSON* changes_report = ag_db_get_changes_report();
    if(changes_report && cJSON_GetArraySize(changes_report)) {
         /* Send it to the cloud */
        ao_cloud_msg(ag_getProxyID(), "155", NULL, NULL, ao_cloud_measures(changes_report, ag_getProxyID()), buf, sizeof(buf));
        pu_queue_push(to_proxy, buf, strlen(buf)+1);
        /* Send it to WS */
        send_to_ws(ao_ws_params(changes_report, buf, sizeof(buf)));
    }
    if(changes_report) cJSON_Delete(changes_report);
    IP_CTX_(30001);
}

static void run_actions() {
    IP_CTX_(17000);
    run_agent_actions();        /* Agent connet/reconnect */
    run_ws_actions();           /* WS connect/reconnect */
    run_streaming_actions();    /* start/stop streaming */
    run_cam_actions();          /* MD/SD on-off; update cam's channges, make snapshot,...*/
    send_reports();             /* Send changes report to the cloud and to WS */
    ag_db_save_persistent();    /* Save persistent data on disk */
    ag_clear_flags();           /* clear change flags to be prepared to the next cycle */
    IP_CTX_(17001);
}

static protocol_type_t get_protocol_number(pu_queue_event_t ev, msg_obj_t* obj) {
    IP_CTX_(18000);
    if(!obj) return MT_PROTO_UNDEF;
    IP_CTX_(18001);
    if(cJSON_GetObjectItem(obj, "gw_cloudConnection") != NULL) return MT_PROTO_OWN;
    IP_CTX_(18002);
    if(cJSON_GetObjectItem(obj, "commands") != NULL) return MT_PROTO_CLOUD;
    IP_CTX_(18003);
    if(ev == AQ_FromWS) return MT_PROTO_WS;
    IP_CTX_(18004);
    if(ev == AQ_FromRW) return MT_PROTO_STREAMING;
    IP_CTX_(18005);
    if(ev == AQ_FromCam) return MT_PROTO_EM;
    IP_CTX_(18006);
    return MT_PROTO_UNDEF;
}
/*
 * TODO Change to common ag_db staff
 */
static void process_own_proxy_message(msg_obj_t* own_msg) {
    IP_CTX_(19000);
    t_ao_msg data;

    ao_proxy_decode(own_msg, &data);
    switch (data.command_type) {
        case AO_IN_CONNECTION_INFO: {
            int info_changed = 0;

            if(data.in_connection_state.is_online) {
                if (strcmp(ag_getProxyID(), data.in_connection_state.proxy_device_id) != 0) {
                    pu_log(LL_INFO, "%s:Proxy sent new ProxyID. Old one = %s, New one = %s.", AT_THREAD_NAME,
                           ag_getProxyID(), data.in_connection_state.proxy_device_id);
                    ag_saveProxyID(data.in_connection_state.proxy_device_id);
                    info_changed = 1;
                }
                if (strcmp(ag_getProxyAuthToken(), data.in_connection_state.proxy_auth) != 0) {
                    pu_log(LL_INFO, "%s:Proxy sent new Auth token. Old one = %s, New one = %s.", AT_THREAD_NAME,
                           ag_getProxyAuthToken(), data.in_connection_state.proxy_auth);
                    ag_saveProxyAuthToken(data.in_connection_state.proxy_auth);
                    info_changed = 1;
                }
                if (strcmp(ag_getMainURL(), data.in_connection_state.main_url) != 0) {
                    pu_log(LL_INFO, "%s:Proxy sent new Main URL. Old one = %s, New one = %s.", AT_THREAD_NAME,
                           ag_getMainURL(), data.in_connection_state.main_url);
                    ag_saveMainURL(data.in_connection_state.main_url);
                    info_changed = 1;
                }
/* Send MD/SD/SNAPHOTS which weren't sent during the regular procedure */
/* If we got conn data - Proxy lost connection. So it is possible we could not send files... */
                send_remaining_files();
            }
            if(info_changed) ag_db_set_int_property(AG_DB_STATE_AGENT_ON, 1);
        }
            break;
        default:
            pu_log(LL_INFO, "%s: Message type = %d. Message ignored", AT_THREAD_NAME, data.command_type);
    }
    IP_CTX_(19001);
}
static void process_cloud_message(msg_obj_t* obj_msg) {
    IP_CTX_(20007);
    int is_ack = ao_proxy_ack_required(obj_msg);
    msg_obj_t* commands = pr_get_cmd_array(obj_msg);
    size_t i;
    for (i = 0; i < pr_get_array_size(commands); i++) {
        msg_obj_t* cmd = pr_get_arr_item(commands, i);
        if(is_ack) send_ACK_to_Proxy(ao_proxy_get_cmd_no(cmd));
        msg_obj_t* params = ao_proxy_get_cloud_params_array(cmd);
        size_t j;
        for(j = 0; j < pr_get_array_size(params); j++) {
            msg_obj_t* param = pr_get_arr_item(params, j);
            const char* param_name = ao_proxy_get_cloud_param_name(param);
            const char* param_value = ao_proxy_get_cloud_param_value(param);
            ag_db_set_property(param_name, param_value);
        }
    }
    IP_CTX_(20008);
}
static void process_ws_message(msg_obj_t* obj_msg) {
    IP_CTX_(20021);
    msg_obj_t* result_code = cJSON_GetObjectItem(obj_msg, "resultCode");
    if(result_code) {
        if(result_code->valueint == AO_WS_PING_RC) {
            ag_db_set_int_property(AG_DB_CMD_PONG_REQUEST, 1); /* Ping received - Pong should be sent */
        }
        else if((result_code->valueint == AO_WS_THREAD_ERROR)||(result_code->valueint == AO_WS_TO_ERROR)) {
            ag_db_set_int_property(AG_DB_STATE_WS_ON, 1); /* WS thread error - reconnect required */
        }
    }
/* First, lets store change on viewersCount and  pingInterval (if any). */
    msg_obj_t* viewers_count = cJSON_GetObjectItem(obj_msg, AG_DB_STATE_VIEWERS_COUNT);
    if(viewers_count) {
        ag_db_set_int_property(AG_DB_STATE_VIEWERS_COUNT, viewers_count->valueint);
    }
    msg_obj_t* ping_interval = cJSON_GetObjectItem(obj_msg, AG_DB_STATE_PING_INTERVAL);
    if(ping_interval) ag_db_set_int_property(AG_DB_STATE_PING_INTERVAL, ping_interval->valueint);
    msg_obj_t* params = ao_proxy_get_ws_array(obj_msg, "params");
    if(params) {
        size_t i;
        for (i = 0; i < pr_get_array_size(params); i++) {
            msg_obj_t *param = pr_get_arr_item(params, i);
            const char *param_name = ao_proxy_get_ws_param_name(param);
            const char *param_value = ao_proxy_get_ws_param_value(param);
            ag_db_set_property(param_name, param_value);
        }
    }
    msg_obj_t* viewers = ao_proxy_get_ws_array(obj_msg, "viewers");
    if(viewers) {
        size_t i;
        for (i = 0; i < pr_get_array_size(viewers); i++) {
            cJSON* v = cJSON_GetArrayItem(viewers, (int)i);
            cJSON* status = cJSON_GetObjectItem(v, "status");
            int vc = ag_db_get_int_property(AG_DB_STATE_VIEWERS_COUNT);
            if((status) && (status->type == cJSON_Number)) {
                switch (status->valueint) {
                    case -1:
                        if(vc--) ag_db_set_int_property(AG_DB_STATE_VIEWERS_COUNT, vc);
                        break;
                    case 0:
                        if(!vc) ag_db_set_int_property(AG_DB_STATE_VIEWERS_COUNT, 1);
                        break;
                    case 1:
                        ag_db_set_int_property(AG_DB_STATE_VIEWERS_COUNT, vc+1);
                        break;
                    default:
                        break;
                }
            }
        }
    } /* Work with viewers */
    IP_CTX_(20022);
}
static void process_em_message(msg_obj_t* obj_msg) {
    IP_CTX_(23000);
    t_ao_cam_alert data = ao_cam_decode_alert(obj_msg);
    pu_log(LL_DEBUG, "%s: Camera alert %d came", __FUNCTION__, data.cam_event);

    switch (data.cam_event) {
        case AC_CAM_START_MD:
            ag_db_set_int_property(AG_DB_STATE_MD_ON, 1);
            ag_db_set_int_property(AG_DB_STATE_RECORDING, 1);
            break;
        case AC_CAM_START_SD:
            ag_db_set_int_property(AG_DB_STATE_SD_ON, 1);
            ag_db_set_int_property(AG_DB_STATE_RECORDING, 1);
            break;
        case AC_CAM_STOP_MD:
            ag_db_set_int_property(AG_DB_STATE_MD_ON, 0);
            ag_db_set_int_property(AG_DB_STATE_RECORDING, 0);

            send_send_file(data);
            break;
        case AC_CAM_STOP_SD:
            ag_db_set_int_property(AG_DB_STATE_SD_ON, 0);
            ag_db_set_int_property(AG_DB_STATE_RECORDING, 0);

            send_send_file(data);
            break;
        case AC_CAM_START_IO:
        case AC_CAM_STOP_IO:
            pu_log(LL_ERROR, "%s: IO event came. Unsupported for now. Ignored", __FUNCTION__);
            break;
        case AC_CAM_STOP_SERVICE:
            pu_log(LL_ERROR, "%s: Camera Events Monitor is out of service. Restart.", __FUNCTION__);
            sleep(1);   /* Just on case... */
            restart_events_monitor();
        case AC_CAM_TIME_TO_PING:
            break;
        default:
            pu_log(LL_ERROR, "%s: Unrecognized alert type %d received. Ignored", data.cam_event);
            break;
    }
    IP_CTX_(23001);
}
static void process_streaming_message(msg_obj_t* obj_msg) {
    IP_CTX_(21000);
    char error_code[10] = {0};
    char err_message[256] = {0};
    msg_obj_t* rc = cJSON_GetObjectItem(obj_msg, "resultCode");
    if(!rc) {
        char* txt = cJSON_PrintUnformatted(obj_msg);
        pu_log(LL_ERROR, "%s: Can't process message %s from streaming thread", __FUNCTION__, (txt)?txt:"Can't convert it to text");
        if(txt)free(txt);
        strncpy(error_code, "internal", sizeof(error_code)-1);
    }
    else
        snprintf(error_code, sizeof(error_code)-1, "%d", rc->valueint);

    snprintf(err_message, sizeof(err_message), "Streaming error %s. Stream restarts.", error_code);
    ac_set_stream_error(err_message);
/* Set the reconnect flag for streamer */
    ag_db_set_int_property(AG_DB_CMD_CONNECT_RW, 1);
    IP_CTX_(21001);
}
/*
 * Process PROXY(CLOUD) & WS messages: OWN from Proxy or parameter by parameter from CLOUD/WS
 */
static void process_message(pu_queue_event_t ev, char* msg) {
    IP_CTX_(20000);
    msg_obj_t *obj_msg = pr_parse_msg(msg);
    if (obj_msg == NULL) {
        pu_log(LL_ERROR, "%s: Error JSON parser on %s. Message ignored.", __FUNCTION__, msg);
        return;
    }
    protocol_type_t protocol_number = get_protocol_number(ev, obj_msg);
    switch (protocol_number) {
        case MT_PROTO_OWN:
            process_own_proxy_message(obj_msg);
            break;
        case MT_PROTO_CLOUD:
            process_cloud_message(obj_msg);
            break;
        case MT_PROTO_WS:
            process_ws_message(obj_msg);
            break;
        case MT_PROTO_EM:
            process_em_message(obj_msg);
            break;
        case MT_PROTO_STREAMING:
            process_streaming_message(obj_msg);
            break;
        default:
            pu_log(LL_ERROR, "%s: Unsupported protocol type %d", __FUNCTION__, protocol_number);
            break;
    }
    pr_erase_msg(obj_msg);
    IP_CTX_(20001);
}

static int main_thread_startup() {
    IP_CTX_(24000);
    aq_init_queues();

    from_poxy = aq_get_gueue(AQ_FromProxyQueue);        /* proxy_read -> main_thread */
    to_proxy = aq_get_gueue(AQ_ToProxyQueue);           /* main_thred -> proxy_write */
    to_wud = aq_get_gueue(AQ_ToWUD);                    /* main_thread -> proxy_write */
    from_cam = aq_get_gueue(AQ_FromCam);                /* cam_control -> main_thread */
    from_ws = aq_get_gueue(AQ_FromWS);                  /* WS -> main_thread */
    from_stream_rw = aq_get_gueue(AQ_FromRW);           /* Streaming RW tread(s) -> main */
    to_sf = aq_get_gueue(AQ_ToSF);                      /* main -> SF thread */


    events = pu_add_queue_event(pu_create_event_set(), AQ_FromProxyQueue);
    events = pu_add_queue_event(events, AQ_FromCam);
    events = pu_add_queue_event(events, AQ_FromWS);
    events = pu_add_queue_event(events, AQ_FromRW);

/* Camera initiation & settings upload */
    if(!ac_cam_init()) {
        pu_log(LL_ERROR, "%s, Error Camera initiation", __FUNCTION__);
        IP_CTX_(24001);
        return 0;
    }
    pu_log(LL_INFO, "%s: Camera initiaied", __FUNCTION__);
    if(!ag_db_load_cam_properties()) {
        pu_log(LL_ERROR, "%s: Error load camera properties", __FUNCTION__);
        IP_CTX_(24002);
        return 0;
    }
    pu_log(LL_INFO, "%s: Settings are loaded, Camera initiated", __FUNCTION__);

/* Threads start */
    if(!at_start_proxy_rw()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", __FUNCTION__, "PROXY_RW", strerror(errno));
        IP_CTX_(24003);
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "PROXY_RW");

    if(!at_start_wud_write()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", __FUNCTION__, "WUD_WRITE", strerror(errno));
        IP_CTX_(24004);
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "WUD_WRITE");

    if(!at_start_sf()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", __FUNCTION__, "FILES_SENDER", strerror(errno));
        IP_CTX_(24005);
        return 0;
    }
    pu_log(LL_INFO, "%s: %s started", "FILES_SENDER", __FUNCTION__);

    if(!at_start_cam_alerts_reader()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", __FUNCTION__, "CAM_ALERT_READED", strerror(errno));
        IP_CTX_(24006);
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "CAM_ALERT_READER", __FUNCTION__);
    IP_CTX_(24007);
    return 1;
}
static void main_thread_shutdown() {
    IP_CTX_(25000);
    ac_stop_video();
    ac_disconnect_video();

    at_set_stop_proxy_rw();
    at_set_stop_wud_write();

    at_stop_cam_alerts_reader();
    at_stop_sf();
    at_stop_wud_write();
    at_stop_proxy_rw();

    ag_db_unload_cam_properties();
    ac_cam_deinit();

    aq_erase_queues();
}
/****************************************************************************************
    Global function definition
*/
void at_main_thread() {
    IP_CTX_(26000);
    main_finish = 0;

    if(!main_thread_startup()) {
        pu_log(LL_ERROR, "%s: Initialization failed. Abort", AT_THREAD_NAME);
        main_finish = 1;
    }
    pu_log(LL_INFO, "%s: Main thread startup Finished OK", AT_THREAD_NAME);

    lib_timer_clock_t wd_clock = {0};           /* timer for watchdog sending */
    lib_timer_init(&wd_clock, ag_getAgentWDTO());   /* Initiating the timer for watchdog sendings */

    lib_timer_clock_t va_clock = {0};   /* timer for viewers amount check */
    lib_timer_init(&va_clock, DEFAULT_AV_ASK_TO_SEC);

    unsigned int events_timeout = 1; /* Wait 1 second */

    pu_log(LL_DEBUG, "%s: Main thread starts", __FUNCTION__);

    while(!main_finish) {
        IP_CTX_(26001);
        size_t len = sizeof(mt_msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;
        pu_queue_t** q=NULL;
         switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case AQ_FromProxyQueue:
                q = &from_poxy;
                break;
            case AQ_FromWS:
                q = &from_ws;
                break;
            case AQ_FromRW:
                q = &from_stream_rw;
                break;
            case AQ_FromCam:
                q = &from_cam;
                break;
             case AQ_Timeout:
                 q = NULL;
                 break;
             case AQ_STOP:
                 q = NULL;
                 main_finish = 1;
                 pu_log(LL_INFO, "%s received STOP event. Terminated", AT_THREAD_NAME);
                 break;
             default:
                 q = NULL;
                 pu_log(LL_ERROR, "%s: Undefined event %d on wait. Message = %s", AT_THREAD_NAME, ev, mt_msg);
                 break;
         }
         if(q) {
             IP_CTX_(26002);
             while(pu_queue_pop(*q, mt_msg, &len)) {
                 pu_log(LL_DEBUG, "%s: got message from the %s: %s", AT_THREAD_NAME, aq_event_2_char(ev), mt_msg);
                 process_message(ev, mt_msg);
                 len = sizeof(mt_msg);
              }
             IP_CTX_(26003);
         }
         /* Place for own periodic actions */
        /*1. Wathchdog */
        if(lib_timer_alarm(wd_clock)) {
            ag_db_set_int_property(AG_DB_CMD_SEND_WD_AGENT, 1);
            lib_timer_init(&wd_clock, ag_getAgentWDTO());
        }
        /*2. Ask viewer about active viewers */
        if(lib_timer_alarm(va_clock)) {
            ag_db_set_int_property(AG_DB_CMD_ASK_4_VIEWERS_WS, 1);
            lib_timer_init(&va_clock, DEFAULT_AV_ASK_TO_SEC);
        }
/* Interpret changes made in properties */
        run_actions();
    }
    main_thread_shutdown();
    pu_log(LL_INFO, "%s: STOP. Terminated", AT_THREAD_NAME);
    pthread_exit(NULL);
}
