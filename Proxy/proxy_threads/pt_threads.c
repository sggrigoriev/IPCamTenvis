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
    Created by gsg on 03/11/16.
*/
#include <pthread.h>
#include <errno.h>
#include <memory.h>

#include "lib_timer.h"
#include "pt_queues.h"
#include "lib_tcp.h"
#include "pr_commands.h"

#include "pc_defaults.h"
#include "pc_settings.h"
#include "ph_manager.h"
#include "pt_server_read.h"
#include "pt_server_write.h"
#include "pt_main_agent.h"
#ifndef PROXY_SEPARATE_RUN
    #include "pt_wud_write.h"
#endif

#include "pt_threads.h"


#define PT_THREAD_NAME "MAIN_THREAD"

/*******************************************************************************
 * Local functions
 */
static int main_thread_startup();                       /* Total Proxy therads startup */
static void main_thread_shutdown();                     /* Total Proxy shutdown */
static void send_device_id_to_agent();                  /* Inform the Agent about Proxy's device id */
static void send_fw_version_to_cloud();                 /* Inform the cloud about the gateway firmware version */
static void send_reboot_status(pr_reboot_param_t status); /* Sand the alert to the cloud: bofore reboot and after reboot */
static void process_cloud_message(char* msg);       /* Parse the info from the clud and sends ACKs to Agent and/or provede commands to the Proxy */
static void process_proxy_commands(char* msg);      /* Process Proxy commands - commsnd(s) string as input */
static void report_cloud_conn_status(int online);         /* send off/on line status notification to the Agent, sent conn info to the WUD if online */

#ifndef PROXY_SEPARATE_RUN
static void send_wd();          /* Send Watchdog to thw WUD */
#endif
/***********************************************************************************************
 * Main Proxy thread data
 */
static pu_queue_msg_t mt_msg[PROXY_MAX_MSG_LEN];    /* The only main thread's buffer! */

static pu_queue_t* from_server;     /* server_read -> main_thread */
static pu_queue_t* to_server;       /* main_thread -> server_write */
static pu_queue_t* to_agent;        /* main_thread -> agent_write */

#ifndef PROXY_SEPARATE_RUN
    static pu_queue_t* to_wud;                  /* main_thread -> wud_write */
    lib_timer_clock_t wd_clock = {0};           /* timer for watchdog sending */
#endif

lib_timer_clock_t cloud_url_update_clock = {0};         /*  timer for contact URL request sending */
lib_timer_clock_t gw_fw_version_sending_clock = {0};    /* timer for firmware version info sending */
/*TODO! If fw upgrade failed lock should be set to 0 again!!! */
int lock_gw_fw_version_sending = 0;     /* set to 1 when the fw upgrade starts. */

int proxy_is_online = 0;                /* 0 - no connection to the cloud; 1 - got connection to the cloud */

static pu_queue_event_t events;         /* main thread events set */

static volatile int main_finish;        /* stop flag for main thread */
/*********************************************************************************************
 * Public function implementation
 */
void pt_main_thread() { /* Starts the main thread. */

    main_finish = 0;

    if(!main_thread_startup()) {
        pu_log(LL_ERROR, "%s: Initialization failed. Abort", PT_THREAD_NAME);
        main_finish = 1;
    }
    unsigned int events_timeout = 0; /* Wait until the end of univerce */
#ifndef PROXY_SEPARATE_RUN
    events_timeout = 1;
    lib_timer_init(&wd_clock, pc_getProxyWDTO());   /* Initiating the timer for watchdog sendings */
#endif
    lib_timer_init(&cloud_url_update_clock, pc_getCloudURLTOHrs()*3600);        /* Initiating the tomer for cloud URL request TO */
    lib_timer_init(&gw_fw_version_sending_clock, pc_getFWVerSendToHrs()*3600);

    send_device_id_to_agent();      /* sending the device id to agent */
    report_cloud_conn_status(proxy_is_online);    /* sending to the agent offline status - no connection with the cloud */
    send_fw_version_to_cloud();     /* sending the fw version to the cloud */
    send_reboot_status(PR_AFTER_REBOOT); /* sending the reboot status to the cloud */

    while(!main_finish) {
        size_t len = sizeof(mt_msg);
        pu_queue_event_t ev;

        switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case PS_Timeout:
                break;
            case PS_FromServerQueue:
                while(pu_queue_pop(from_server, mt_msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the cloud %s", PT_THREAD_NAME, mt_msg);
                    process_cloud_message(mt_msg);
                     len = sizeof(mt_msg);
                }
                break;
            case PS_STOP:
                main_finish = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", PT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait (from agent / from server)!", PT_THREAD_NAME, ev);
                break;
        }
/* Place for own periodic actions */
/*1. Wathchdog */
#ifndef PROXY_SEPARATE_RUN
        if(lib_timer_alarm(wd_clock)) {
            send_wd();
            lib_timer_init(&wd_clock, pc_getProxyWDTO());
        }
#endif
/*2. Regular contact url update */
        if(lib_timer_alarm(cloud_url_update_clock)) {
            pu_log(LL_INFO, "%s going to update the contact cloud URL...", PT_THREAD_NAME);
            proxy_is_online = 0;
            report_cloud_conn_status(proxy_is_online);
            ph_update_contact_url();
            proxy_is_online = 1;
            report_cloud_conn_status(proxy_is_online);

            lib_timer_init(&cloud_url_update_clock, pc_getCloudURLTOHrs()*3600);
        }
/* 3. Regular sending the fw version to the cloud */
        if(lib_timer_alarm(gw_fw_version_sending_clock) && !lock_gw_fw_version_sending) {
            send_fw_version_to_cloud();
            lib_timer_init(&gw_fw_version_sending_clock, pc_getFWVerSendToHrs()*3600);
        }
    }
    main_thread_shutdown();
}

/***************************************************************************************************
 * Local functions implementation
 */
static int main_thread_startup() {
    init_queues();

    from_server = pt_get_gueue(PS_FromServerQueue);
    to_server = pt_get_gueue(PS_ToServerQueue);
    to_agent = pt_get_gueue(PS_ToAgentQueue);

#ifndef PROXY_SEPARATE_RUN
    to_wud = pt_get_gueue(PS_ToWUDQueue);
#endif

    events = pu_add_queue_event(pu_create_event_set(), PS_FromAgentQueue);
    events = pu_add_queue_event(events, PS_FromServerQueue);

    if(!start_server_read()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", PT_THREAD_NAME, "SERVER_READ", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "SERVER_READ");

    if(!start_server_write()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", PT_THREAD_NAME, "SERVER_WRITE", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "SERVER_WRITE");

    if(!start_agent_main()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", PT_THREAD_NAME, "AGENT_MAIN", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "AGENT_MAIN");

#ifndef PROXY_SEPARATE_RUN
    if(!start_wud_write()) {
        pu_log(LL_ERROR, "%s: Creating %s failed: %s", PT_THREAD_NAME, "WUD_WRITE", strerror(errno));
        return 0;
    }
    pu_log(LL_INFO, "%s: started", "WUD_WRITE");

    return 1;
#else
    return 1;
#endif
}

static void main_thread_shutdown() {
    set_stop_server_read();
    set_stop_server_write();
    set_stop_agent_main();

#ifndef PROXY_SEPARATE_RUN
    set_stop_wud_write();
#endif

    stop_server_read();
    stop_server_write();
    stop_agent_main();

#ifndef PROXY_SEPARATE_RUN
    stop_wud_write();
#endif
    erase_queues();
}

#ifndef PROXY_SEPARATE_RUN
    /* Send Watchdog to the WUD */
    static void send_wd() {
        char buf[LIB_HTTP_MAX_MSG_SIZE];
        char di[LIB_HTTP_DEVICE_ID_SIZE];

        pc_getProxyDeviceID(di, sizeof(di));
        pr_make_wd_alert4WUD(buf, sizeof(buf), pc_getProxyName(), di);

        pu_queue_push(to_wud, buf, strlen(buf)+1);
    }
#endif
/* Send DeviceID to the Agent */
static void send_device_id_to_agent() {
    const char* first_msg_part = "{\"gw_gatewayDeviceId\":[{\"paramsMap\":{\"deviceId\":\"";
    const char* second_msg_part = "\"}}]}";

    char device_id[LIB_HTTP_DEVICE_ID_SIZE];
    char msg[LIB_HTTP_MAX_MSG_SIZE];
    pc_getProxyDeviceID(device_id, sizeof(device_id));

    snprintf(msg, sizeof(msg)-1, "%s%s%s", first_msg_part, device_id, second_msg_part);

    pu_queue_push(to_agent, msg, strlen(msg)+1);
    pu_log(LL_INFO, "%s: device id was sent to the Agent: %s", PT_THREAD_NAME, msg);

}

/*Sending the fw version to the cloud accordingly to the schedule */
static void send_fw_version_to_cloud() {
    char device_id[LIB_HTTP_DEVICE_ID_SIZE];
    char fw_ver[DEFAULT_FW_VERSION_SIZE];
    char msg[LIB_HTTP_MAX_MSG_SIZE];

    pc_getProxyDeviceID(device_id, sizeof(device_id));
    pc_getFWVersion(fw_ver, sizeof(fw_ver));

    pr_make_fw_status4cloud(msg, sizeof(msg), PR_FWU_STATUS_STOP, fw_ver, device_id);


    pu_queue_push(to_server, msg, strlen(msg)+1);
    pu_log(LL_INFO, "%s: firmware version was sent to the Cloud: %s", PT_THREAD_NAME, msg);
}

static void send_reboot_status(pr_reboot_param_t status) {
    char device_id[LIB_HTTP_DEVICE_ID_SIZE];
    char msg[LIB_HTTP_MAX_MSG_SIZE];

    pc_getProxyDeviceID(device_id, sizeof(device_id));

    pr_make_reboot_alert4cloud(msg, sizeof(msg), device_id, status);

    pu_queue_push(to_server, msg, strlen(msg)+1);
    pu_log(LL_INFO, "%s: reboot status was sent to the Cloud: %s", PT_THREAD_NAME, msg);
}

static void process_cloud_message(char* cloud_msg) {
    msg_obj_t* msg = pr_parse_msg(cloud_msg);
    if(!msg) {
        pu_log(LL_ERROR, "%s: Incoming message %s ignored", PT_THREAD_NAME, cloud_msg);
        return;
    }
    if(pr_get_message_type(msg) != PR_COMMANDS_MSG) { /* currently we're not make business with alerts and/or measuruments in Proxy */
        pr_erase_msg(msg);
        pu_log(LL_INFO, "%s: message from cloud to Agent: %s", PT_THREAD_NAME, cloud_msg);
        pu_queue_push(to_agent, cloud_msg, strlen(cloud_msg)+1);
    }
    else {      /* Here are commands! */
        char for_agent[LIB_HTTP_MAX_MSG_SIZE]={0};
        char for_proxy[LIB_HTTP_MAX_MSG_SIZE]={0};

        char device_id[LIB_HTTP_DEVICE_ID_SIZE];

        pc_getProxyDeviceID(device_id, sizeof(device_id));

/* Separate the info berween Proxy & Agent */
        pr_split_msg(msg, device_id, for_proxy, sizeof(for_proxy), for_agent, sizeof(for_agent));
        pr_erase_msg(msg);
        if(strlen(for_agent)) {
            pu_log(LL_INFO, "%s: from cloud to Agent: %s", PT_THREAD_NAME, for_agent);
            pu_queue_push(to_agent, for_agent, strlen(for_agent)+1);
        }
        if(strlen(for_proxy)) {
            pu_log(LL_INFO, "%s: command(s) array from cloud to Proxy: %s", PT_THREAD_NAME, for_proxy);
            process_proxy_commands(for_proxy);
        }
    }
}

static void process_proxy_commands(char* msg) {
    msg_obj_t* cmd_array = pr_parse_msg(msg);

    if(!cmd_array) {
        pu_log(LL_ERROR, "%s: wrong commands array structure in message %s. Ignored", PT_THREAD_NAME, msg);
        return;
    }
    size_t size = pr_get_array_size(cmd_array);
    if(!size) {
        pu_log(LL_WARNING, "%s: empty proxy commands array in message %s. Ignored", PT_THREAD_NAME, msg);
        pr_erase_msg(cmd_array);
        return;
    }
    size_t i;
    for(i = 0; i < size; i++) {
        msg_obj_t* cmd_arr_elem = pr_get_arr_item(cmd_array, i);    /* Get Ith command */
        pr_cmd_item_t cmd_item = pr_get_cmd_item(cmd_arr_elem);     /* Get params of Ith command */
        switch (cmd_item.command_type) {
#ifndef PROXY_SEPARATE_RUN
            case PR_CMD_FWU_START: {
                lock_gw_fw_version_sending = 1;

/*            case PR_CMD_FWU_CANCEL:    // And who will initiate the cancellation??? */
                char for_wud[LIB_HTTP_MAX_MSG_SIZE];
                msg_obj_t* cmd_array = pr_make_cmd_array(cmd_arr_elem);
                pr_obj2char(cmd_array, for_wud, sizeof(for_wud));
                pu_queue_push(to_wud, for_wud, strlen(for_wud)+1);
                pr_erase_msg(cmd_array);
                break;
            }
            case PR_CMD_STOP:
                pu_log(LL_INFO, "%s: finished because of %s", PT_THREAD_NAME, msg);
                main_finish = 1;
#endif
            case PR_CMD_UPDATE_MAIN_URL:
                proxy_is_online = 0;
                report_cloud_conn_status(proxy_is_online);
                 if(!ph_update_main_url(cmd_item.update_main_url.main_url)) {
                    pu_log(LL_ERROR, "%s: Main URL update failed", PT_THREAD_NAME);
                }
                proxy_is_online = 1;
                report_cloud_conn_status(proxy_is_online);
                break;
            case PR_CMD_REBOOT: {    /* The cloud kindly asks to shut up & reboot */
                pu_log(LL_INFO, "%s: CLoud command REBOOT received", PT_THREAD_NAME);

                char for_wud[LIB_HTTP_MAX_MSG_SIZE];
                msg_obj_t* cmd_array = pr_make_cmd_array(cmd_arr_elem);
                pr_obj2char(cmd_array, for_wud, sizeof(for_wud));
                pu_queue_push(to_wud, for_wud, strlen(for_wud) + 1);
                pr_erase_msg(cmd_array);
                break;
            }
            case PR_CMD_CLOUD_CONN: /* reconnection case */
                if(!proxy_is_online) {  /* proxy was offline and now got new connecion! */
                    pu_log(LL_INFO, "%s: Proxy connected with the cloud", PT_THREAD_NAME);
                    proxy_is_online = 1;
                    report_cloud_conn_status(proxy_is_online);
                }
                else {
                    pu_log(LL_INFO, "%s: Secondary connection alert. Ignored", PT_THREAD_NAME);
                }
                break;
            case PR_CMD_CLOUD_OFF:
                if(proxy_is_online) {   /* connection disapeared - has to notify about it */
                    pu_log(LL_INFO, "%s: Proxy disconnected with the cloud", PT_THREAD_NAME);
                    proxy_is_online = 0;
                    report_cloud_conn_status(proxy_is_online);
                }
                else {
                    pu_log(LL_INFO, "%s: Secondary disconnection alert. Ignored", PT_THREAD_NAME);
                }
                break;
            case PR_CMD_UNDEFINED:
                pu_log(LL_ERROR, "%s: bad command syntax command %s in msg %s. Ignored.", PT_THREAD_NAME, cmd_arr_elem, msg);
                break;
            default:
                pu_log(LL_ERROR, "%d: unsopported command %s in message %s. Ignored", PT_THREAD_NAME, cmd_arr_elem, msg);
                break;
        }
    }
    pr_erase_msg(cmd_array);
}

static void report_cloud_conn_status(int online) {
    char buf[LIB_HTTP_MAX_MSG_SIZE] = {0};
    char deviceID[LIB_HTTP_DEVICE_ID_SIZE] = {0};
/* 1. Send the alert to the Agent */

    pc_getProxyDeviceID(deviceID, sizeof(deviceID));
    pr_conn_state_notf_to_agent(buf, sizeof(buf), deviceID, online);
    pu_queue_push(to_agent, buf, strlen(buf)+1);

/* 2. if the status == online - send the connection info to the WUD */
    if(online) {
        char conn_string[LIB_HTTP_MAX_URL_SIZE] = {0};
        char auth_token[LIB_HTTP_AUTHENTICATION_STRING_SIZE] = {0};
        char fw_version[LIB_HTTP_FW_VERSION_SIZE] = {0};
        char local_ip_address[LIB_HTTP_MAX_IPADDRES_SIZE] = {0};
        char interface[LIB_HTTP_MAX_IP_INTERFACE_SIZE] = {0};

        pc_getCloudURL(conn_string, sizeof(conn_string));
        pc_getAuthToken(auth_token, sizeof(auth_token));
        pc_getFWVersion(fw_version, sizeof(fw_version));

        pr_make_conn_info_cmd(buf, sizeof(buf), conn_string, deviceID, auth_token, fw_version);
        pu_queue_push(to_wud, buf, strlen(buf)+1);
/* 3. Send the local IP-address to the cloud. */
#ifdef PROXY_ETHERNET_INTERFACE
        strncpy(interface, PROXY_ETHERNET_INTERFACE, sizeof(interface)-1);
#endif
        lib_tcp_local_ip(interface, local_ip_address, sizeof(local_ip_address));
        if(strlen(local_ip_address)) {
            pr_make_local_ip_notification(buf, sizeof(buf), local_ip_address, deviceID);
            pu_queue_push(to_server, buf, strlen(buf)+1);
        }
        else {
            pu_log(LL_WARNING, "report_cloud_conn_status: No info about local IP Address. Check PROXY_ETHERNET_INTERFACE define in Make file!");
        }
    }
}