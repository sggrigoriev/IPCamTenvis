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

static pu_queue_msg_t mt_msg[LIB_HTTP_MAX_MSG_SIZE];    /* The only main thread's buffer! */
static pu_queue_event_t events;         /* main thread events set */
static pu_queue_t* from_poxy;       /* proxy_read -> main_thread */
static pu_queue_t* to_proxy;        /* main_thread->proxy */
static pu_queue_t* to_wud;          /* main_thread -> wud_write  */
static pu_queue_t* from_cam;        /* cam -> main_thread */
static pu_queue_t* from_ws;         /* WS -> main_thread */
static pu_queue_t* from_stream_rw;  /* from streaming threads to main */

static volatile int main_finish;        /* stop flag for main thread */

/*****************************************************************************************
    Local functions deinition
*/
static void send_startup_report() {
    cJSON* report = ag_db_get_startup_report();
    if(report) {
        char buf[LIB_HTTP_MAX_MSG_SIZE];
        const char* msg = ao_cloud_measures(report, ag_getProxyID(), buf, sizeof(buf)-1);
        cJSON_Delete(report);
        if(!msg) {
            pu_log(LL_ERROR, "%s: message to cloud exceeds max size %d. Ignored", __FUNCTION__, LIB_HTTP_MAX_MSG_SIZE);
            return;
        }
        pu_queue_push(to_proxy, msg, strlen(msg)+1);
    }
    else {
        pu_log(LL_ERROR, "%s: Error startup report creation. Nothing was sent.", __FUNCTION__);
    }
}
static void send_wd() {
    char buf[LIB_HTTP_MAX_MSG_SIZE];

    pr_make_wd_alert4WUD(buf, sizeof(buf), ag_getAgentName(), ag_getProxyID());
    pu_queue_push(to_wud, buf, strlen(buf)+1);
}
static void send_reboot() {
    char buf[LIB_HTTP_MAX_MSG_SIZE] = {0};

    pu_log(LL_INFO, "%s: Cam Agent requests for reboot", __FUNCTION__);
    pr_make_reboot_command(buf, sizeof(buf), ag_getProxyID());
    pu_queue_push(to_wud, buf, strlen(buf) + 1);
}
static void send_snapshot(const char* full_path) {
    const char* fmt = "\"filesList\": [%s]";
    char flist[256+strlen(fmt)+1];
    char buf[LIB_HTTP_MAX_MSG_SIZE];

    snprintf(flist, sizeof(flist), fmt, full_path);
    pr_make_send_files4WUD(buf, sizeof(buf), flist, ag_getProxyID());
    pu_queue_push(to_wud, buf, strlen(buf) + 1);
}
static void send_send_file(t_ao_cam_alert data) {
    char buf[LIB_HTTP_MAX_MSG_SIZE];
    char f_list[LIB_HTTP_MAX_MSG_SIZE];

    if(strlen(ac_cam_get_files_name(data, f_list, sizeof(f_list))) > 0) {
        pr_make_send_files4WUD(buf, sizeof(buf), f_list, ag_getProxyID());
        pu_queue_push(to_wud, buf, strlen(buf) + 1);
    }
    else {
        pu_log(LL_WARNING, "%s: no alarm files where found - no data set to WUD", AT_THREAD_NAME);
    }
}
static void send_ACK_to_Proxy(int command_number) {
    char buf[128];
    ao_answer_to_command(buf, sizeof(buf), command_number, 0);
    pu_queue_push(to_proxy, buf, strlen(buf) + 1);
}
static int send_to_ws(const char* msg) {
    if(!at_ws_send(msg)) {
        pu_log(LL_ERROR, "%s: Error sending  %s to WS. Restart WS required", __FUNCTION__, msg);
        ag_db_set_flag_on(AG_DB_CMD_CONNECT_WS);
        return 0;
    }
    return 1;
}
static int send_answers_to_ws() {
    int ret = 0;
    char buf[LIB_HTTP_MAX_MSG_SIZE];
    cJSON* changes_report = ag_db_get_changes_report();
    if(changes_report) {
        if(ao_ws_params(changes_report, buf, sizeof(buf))) {
            ret = send_to_ws(buf);
        }
        cJSON_Delete(changes_report);
    }
    return ret;
}


static void run_agent_actions() {
    if(ag_db_get_flag(AG_DB_STATE_AGENT_ON)) { /* AGENT is ON */
        if (ag_db_get_flag(AG_DB_CMD_CONNECT_AGENT)) {  /* Reconnect case */
            pu_log(LL_DEBUG, "%s: Agent: Connection info changed. WS reconnect requested", __FUNCTION__);
            ag_db_set_flag_on(AG_DB_CMD_CONNECT_WS);  /* Ask for WS (re)connect */
        }
     }
    else {  /* AGENT is off */
        pu_log(LL_DEBUG, "%s: Agent: Disconnected. WS connect requested", __FUNCTION__);
        ag_db_set_flag_on(AG_DB_STATE_AGENT_ON);   /* Set agent status to on */
        send_startup_report();
        ag_db_set_flag_on(AG_DB_CMD_CONNECT_WS);  /* Ask WS connect */
    }
    ag_db_set_flag_off(AG_DB_CMD_CONNECT_AGENT);    /* Clear Agent's connect command */

    if(ag_db_get_flag(AG_DB_CMD_SEND_WD_AGENT)) {
        send_wd();
        ag_db_set_flag_off(AG_DB_CMD_SEND_WD_AGENT);
    }
}
static void run_ws_actions() {
    if(ag_db_get_flag(AG_DB_STATE_WS_ON)) { /* WS is ON */
        if(ac_is_stream_error()) {       /* sent stream error to WS */
            ac_send_stream_error();
            ac_clear_stream_error();
         }
        if(ag_db_get_flag(AG_DB_CMD_CONNECT_WS)) {  /* Reconnect case */
            pu_log(LL_DEBUG, "%s WS: WS is ON. Reconnect requested", __FUNCTION__);
            ac_disconnect_video();
            ag_db_set_flag_off(AG_DB_STATE_WS_ON);
            ag_db_set_flag_on(AG_DB_CMD_DISCONNECT_RW); /* In case we got live streaming */

            if (!ac_connect_video()) {
                pu_log(LL_ERROR, "%s: Error WS interface start. WS inactive.", __FUNCTION__);
            } else {
                ag_db_set_flag_on(AG_DB_STATE_WS_ON);
                ag_db_set_flag_off(AG_DB_CMD_CONNECT_WS);   /* Clear command as executed */
            }
        }
    }
    else {  /* WS is off */
        if(ag_db_get_flag(AG_DB_CMD_CONNECT_WS)) {    /* Connect WS */
            pu_log(LL_DEBUG, "%s: WS is OFF. Connect requested", __FUNCTION__);
            if(!ac_connect_video()) {
                pu_log(LL_ERROR, "%s: Error WS interface start. WS inactive.", __FUNCTION__);
            }
            else {
                ag_db_set_flag_on(AG_DB_STATE_WS_ON);
                ag_db_set_flag_off(AG_DB_CMD_CONNECT_WS);   /* Clear command as executed */
            }
        }
    }
/* Check if viewers count request happens */
    if(ag_db_get_flag(AG_DB_CMD_ASK_4_VIEWERS_WS) && ag_db_get_flag(AG_DB_STATE_WS_ON)) {
        ac_send_active_viwers_request();                    /* NB! The ws ability to send doesn't checked here! */
        ag_db_set_flag_off(AG_DB_CMD_ASK_4_VIEWERS_WS);
    }
/* Check for ping from WS */
    if(ag_db_get_flag(AG_DB_CMD_PONG_REQUEST)) {
        send_to_ws(ao_answer_to_ws_ping());
        ag_db_set_flag_off(AG_DB_CMD_PONG_REQUEST);
    }
}
static void run_streaming_actions() {
    if(ag_db_get_flag(AG_DB_STATE_RW_ON)) {     /* Streaming works */
        if (ag_db_get_flag(AG_DB_CMD_CONNECT_RW)) {  /* reconnect request */
            pu_log(LL_DEBUG, "%s: Streaming is ON. Restart requested", __FUNCTION__);
            ac_stop_video();
            ag_db_set_flag_off(AG_DB_STATE_RW_ON);
            if (!ac_start_video()) {
                pu_log(LL_ERROR, "%s: Error RW start. RW inactive", __FUNCTION__);
            } else {
                ag_db_set_flag_on(AG_DB_STATE_RW_ON);
                ag_db_set_flag_off(AG_DB_CMD_CONNECT_RW);
            }
        }
        if (ag_db_get_flag(AG_DB_CMD_DISCONNECT_RW) || (ag_db_get_int_property(AG_DB_STATE_VIEWERS_COUNT) == 0)) {/* Stop streaming request */
            pu_log(LL_DEBUG, "%s: Streaming is ON. Active viwers amount is %d. Stop requested", __FUNCTION__, ag_db_get_int_property(AG_DB_STATE_VIEWERS_COUNT));
            ac_stop_video();
            ag_db_set_flag_off(AG_DB_STATE_RW_ON);
            ag_db_set_flag_off(AG_DB_CMD_DISCONNECT_RW);
        }
    }
    else {  /* Streaming is off */
        if(ag_db_get_int_property(AG_DB_STATE_STREAM_STATUS) || (ag_db_get_int_property(AG_DB_STATE_VIEWERS_COUNT) > 0)) {
            ag_db_set_flag_on(AG_DB_CMD_CONNECT_RW);
            if(!ag_db_get_int_property(AG_DB_STATE_VIEWERS_COUNT)) ag_db_store_property(AG_DB_STATE_VIEWERS_COUNT, "1");
        }
        if (ag_db_get_flag(AG_DB_CMD_CONNECT_RW)) {
            pu_log(LL_DEBUG, "%s: Streaming is OFF. Start requested", __FUNCTION__);
            if (!ac_start_video()) {
                pu_log(LL_ERROR, "%s: Error RW start. RW inactive", __FUNCTION__);
            }
            else {
                ag_db_set_flag_on(AG_DB_STATE_RW_ON);
                ag_db_set_flag_off(AG_DB_CMD_CONNECT_RW);
                ag_db_set_flag_off(AG_DB_STATE_STREAM_STATUS); /* Start video sends answers about streaming */
            }
        }
    }
}
static void run_cam_actions() {
/* Switch On/Off MD */
/* Switch On/Off SD */
/* Change Audio sensitivity */
/* Change Video sensitivity */
    ag_db_update_changed_cam_parameters();
}
static void run_snapshot_actions() {
    int snapshot_command = ag_db_get_int_property(AG_DB_STATE_SNAPSHOT);
    if (snapshot_command < 1) return;

    const char *filename = "pictureXXXXXX";
    const char *path = "./";
    char full_path[strlen(filename) + strlen(path) + 1];
    strcpy(full_path, path);
    strcat(full_path, filename);
    mktemp(full_path + strlen(path));
    if (!ac_cam_make_snapshot(full_path)) {
        pu_log(LL_ERROR, "%s: Error picture creation", __FUNCTION__);
        return;
    }
    send_snapshot(full_path);
    ag_db_store_property(AG_DB_STATE_SNAPSHOT, "0");
/*
 * if snapshot_command == 1 the alert should be sent. Was removed as unnecessary
 */
}

static void run_actions() {
    run_agent_actions();        /* Agent connet/reconnect */
    run_ws_actions();           /* WS connect/reconnect */
    run_streaming_actions();    /* start/stop streaming */
    run_snapshot_actions();     /* make a photo */
    run_cam_actions();          /* MD/SD on-off */
    send_answers_to_ws();       /* Send changes report */
}

/*
 * 0 - ERROR
 * 1 - own (proxy)  "gw_cloudConnection"
 * 2 - cloud        "commands"
 * 3 - WS           "params"
 */
static int get_protocol_number(msg_obj_t* obj) {
    if(!obj) return 0;
    if(cJSON_GetObjectItem(obj, "gw_cloudConnection") != NULL) return 1;
    if(cJSON_GetObjectItem(obj, "commands") != NULL) return 2;
    if((cJSON_GetObjectItem(obj, "params")!=NULL) || (cJSON_GetObjectItem(obj, "viewersCount")!=NULL)) return 3;
    return 0;
}
/*
 * TODO Change to common ag_db staff
 */
static void process_own_proxy_message(msg_obj_t* own_msg) {
    t_ao_msg data;

    ao_proxy_decode(own_msg, &data);
    if(data.command_type == AO_IN_CONNECTION_INFO) {
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
        }

        if(info_changed) ag_db_set_flag_on(AG_DB_CMD_CONNECT_AGENT);
    }
    else {
        pu_log(LL_ERROR, "%s: Can't process message from Proxy. Message type = %d. Message ignored", AT_THREAD_NAME, data.command_type);
    }
}
/*
 * Process PROXY(CLOUD) & WS messages: OWN from Proxy or parameter by parameter from CLOUD/WS
 */
static void process_message(char* msg) {
    msg_obj_t *obj_msg = pr_parse_msg(msg);
    if (obj_msg == NULL) {
        pu_log(LL_ERROR, "%s: Error JSON parser on %s. Message ignored.", __FUNCTION__, msg);
        return;
    }
    int protocol_number = get_protocol_number(obj_msg);
    if(!protocol_number) {
        pu_log(LL_ERROR, "%s: protocol for %s unrecognized. Message ignored.", __FUNCTION__, msg);
        cJSON_Delete(obj_msg);
        return;
    }
    switch (protocol_number) {
        case 1:     /* Local message from Proxy to Agent */
            process_own_proxy_message(obj_msg);
            break;
        case 2: {     /* Cloud case */
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
                    ag_db_store_property(param_name, param_value);
                }
            }
        }
            break;
        case 3: {   /* WS case */
            msg_obj_t* result_code = cJSON_GetObjectItem(obj_msg, "resultCode");
            if(result_code && (result_code->valueint == 10)) {      /* Ping received - Pong should be sent */
                ag_db_set_flag_on(AG_DB_CMD_PONG_REQUEST);
            }
/* First, lets store change on viewersCount and  pingInterval (if any). */
            msg_obj_t* viewers_count = cJSON_GetObjectItem(obj_msg, AG_DB_STATE_VIEWERS_COUNT);
            if(viewers_count) ag_db_store_property(AG_DB_STATE_VIEWERS_COUNT, viewers_count->valuestring);

            msg_obj_t* ping_interval = cJSON_GetObjectItem(obj_msg, AG_DB_STATE_PING_INTERVAL);
            if(ping_interval) ag_db_store_property(AG_DB_STATE_PING_INTERVAL, ping_interval->valuestring);

            msg_obj_t* params = ao_proxy_get_ws_params_array(obj_msg);
            if(params) {
                size_t i;
                for (i = 0; i < pr_get_array_size(params); i++) {
                    msg_obj_t *param = pr_get_arr_item(params, i);
                    const char *param_name = ao_proxy_get_ws_param_name(param);
                    const char *param_value = ao_proxy_get_ws_param_value(param);
                    ag_db_store_property(param_name, param_value);
                }
            }
        }
        default:
            pu_log(LL_ERROR, "%s: Unrecognized protocol # %d Message %s ignored.", __FUNCTION__, protocol_number, msg);
            break;
    }
    pr_erase_msg(obj_msg);
}
static void process_rw_message(char* msg) {
  char error_code[10] = {0};
    char err_message[256] = {0};
    msg_obj_t *obj_msg = pr_parse_msg(msg);
    if (obj_msg == NULL) {
        pu_log(LL_ERROR, "%s: Error JSON parser on %s. Message ignored.", __FUNCTION__, msg);
        return;
    }
    msg_obj_t* rc = cJSON_GetObjectItem(obj_msg, "resultCode");
    if(!rc) {
        pu_log(LL_ERROR, "%s: Can't process message %s from streaming thread", __FUNCTION__, msg);
        strncpy(error_code, "internal", sizeof(error_code)-1);
    }
    else
        snprintf(error_code, sizeof(error_code)-1, "%d", rc->valueint);

    snprintf(err_message, sizeof(err_message), "Streaming error %s. Stream restarts.", error_code);
    ac_set_stream_error(err_message);

    ag_db_set_flag_on(AG_DB_CMD_CONNECT_RW);    /* Ask RW for (re) connect */
}

static void restart_events_monitor() {
    at_stop_cam_alerts_reader();
    if(!at_start_cam_alerts_reader()) {
        pu_log(LL_ERROR, "%s: Can't restart Events Monitor. Reboot.", __FUNCTION__);
        send_reboot();
    }
}
static void process_alert(char* msg) {
    t_ao_cam_alert data = ao_cam_decode_alert(msg);
    pu_log(LL_DEBUG, "%s: Camera alert %d came");

    switch (data.cam_event) {
        case AC_CAM_START_MD:
            ag_db_store_property(AG_DB_STATE_MD_ON, "1");
            ag_db_store_property(AG_DB_STATE_RECORDING, "1");
            break;
        case AC_CAM_START_SD:
            ag_db_store_property(AG_DB_STATE_SD_ON, "1");
            ag_db_store_property(AG_DB_STATE_RECORDING, "1");
            break;
        case AC_CAM_STOP_MD:
            ag_db_store_property(AG_DB_STATE_MD_ON, "0");
            ag_db_store_property(AG_DB_STATE_RECORDING, "0");

            send_send_file(data);
            break;
        case AC_CAM_STOP_SD:
            ag_db_store_property(AG_DB_STATE_SD_ON, "0");
            ag_db_store_property(AG_DB_STATE_RECORDING, "0");

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
        default:
            pu_log(LL_ERROR, "%s: Unrecognized alert type %d received. Ignored", data.cam_event);
            break;
    }
}

static int main_thread_startup() {

    aq_init_queues();

    from_poxy = aq_get_gueue(AQ_FromProxyQueue);        /* proxy_read -> main_thread */
    to_proxy = aq_get_gueue(AQ_ToProxyQueue);           /* main_thred -> proxy_write */
    to_wud = aq_get_gueue(AQ_ToWUD);                    /* main_thread -> proxy_write */
    from_cam = aq_get_gueue(AQ_FromCam);                /* cam_control -> main_thread */
    from_ws = aq_get_gueue(AQ_FromWS);                  /* WS -> main_thread */
    from_stream_rw = aq_get_gueue(AQ_FromRW);           /* Streaming RW tread(s) -> main */

    events = pu_add_queue_event(pu_create_event_set(), AQ_FromProxyQueue);
    events = pu_add_queue_event(events, AQ_FromCam);
    events = pu_add_queue_event(events, AQ_FromWS);
    events = pu_add_queue_event(events, AQ_FromRW);

/* Threads start */
    if(!at_start_proxy_rw()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", __FUNCTION__, "PROXY_RW", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "PROXY_RW");

    if(!at_start_wud_write()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", __FUNCTION__, "WUD_WRITE", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "WUD_WRITE");

/* Camera initiation & settings upload */
    if(!ac_cam_init()) {
        pu_log(LL_ERROR, "%s, Error Camera initiation", __FUNCTION__);
        return 0;
    }
    pu_log(LL_INFO, "%s: Camera initiaied", __FUNCTION__);
    if(!ag_db_load_cam_properties()) {
        pu_log(LL_ERROR, "%s: Error load camera properties", __FUNCTION__);
        return 0;
    }
    pu_log(LL_INFO, "%s: Settings are loaded, Camera initiated", __FUNCTION__);

    if(!at_start_cam_alerts_reader()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", __FUNCTION__, "CAM_ALERT_READED", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "CAM_ALERT_READED");

    return 1;
}
static void main_thread_shutdown() {
    ac_stop_video();
    ac_disconnect_video();

    at_set_stop_proxy_rw();
    at_set_stop_wud_write();

    at_stop_cam_alerts_reader();
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
    main_finish = 0;

    if(!main_thread_startup()) {
        pu_log(LL_ERROR, "%s: Initialization failed. Abort", AT_THREAD_NAME);
        main_finish = 1;
    }

    lib_timer_clock_t wd_clock = {0};           /* timer for watchdog sending */
    lib_timer_init(&wd_clock, ag_getAgentWDTO());   /* Initiating the timer for watchdog sendings */

    lib_timer_clock_t ws_clock = {0};   /* timer for WEB viewer check */
    lib_timer_init(&ws_clock, ag_db_get_int_property(AG_DB_STATE_PING_INTERVAL));

    unsigned int events_timeout = 1; /* Wait 1 second */

    while(!main_finish) {
        size_t len = sizeof(mt_msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;

         switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case AQ_FromProxyQueue:
                while(pu_queue_pop(from_poxy, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Proxy %s", AT_THREAD_NAME, mt_msg);
                    process_message(mt_msg);
                    len = sizeof(mt_msg);
                 }
                 break;
            case AQ_FromWS:
                while(pu_queue_pop(from_ws, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Web Socket interface %s, len = %d", AT_THREAD_NAME, mt_msg, len);
                    process_message(mt_msg);
                    len = sizeof(mt_msg);
                }
                break;
            case AQ_FromRW:
                while(pu_queue_pop(from_stream_rw, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the streaming threads %s", AT_THREAD_NAME, mt_msg);
                    process_rw_message(mt_msg);
                    len = sizeof(mt_msg);
                }
                break;
            case AQ_FromCam:
                while(pu_queue_pop(from_cam, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Cam async interface %s", AT_THREAD_NAME, mt_msg);
                    process_alert(mt_msg);
                    len = sizeof(mt_msg);
                }
                break;
            case AQ_Timeout:
//                pu_log(LL_DEBUG, "%s: timeout", AT_THREAD_NAME);
                break;
            case AQ_STOP:
                main_finish = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", AT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait. Message = %s", AT_THREAD_NAME, ev, mt_msg);
                 break;
        }
         /* Place for own periodic actions */
        /*1. Wathchdog */
        if(lib_timer_alarm(wd_clock)) {
            ag_db_set_flag_on(AG_DB_CMD_SEND_WD_AGENT);
            lib_timer_init(&wd_clock, ag_getAgentWDTO());
        }
        /*2. Ask viwer about active viewers - in case of WEB viewer timeout */
        if(lib_timer_alarm(ws_clock) && (ag_db_get_flag(AG_DB_STATE_WS_ON))) {
            ag_db_set_flag_on(AG_DB_CMD_ASK_4_VIEWERS_WS);
            lib_timer_init(&ws_clock, ag_db_get_int_property(AG_DB_STATE_PING_INTERVAL));
        }
/* Interpret changes made in properties */
        run_actions();
    }
    main_thread_shutdown();
    pu_log(LL_INFO, "%s: STOP. Terminated", AT_THREAD_NAME);
    pthread_exit(NULL);
}
