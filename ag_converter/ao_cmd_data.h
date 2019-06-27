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
 Created by gsg on 30/10/17.
 Contains internal presentation for cloud, Proxy & camera messages
 Has to be refactored
*/

#ifndef IPCAMTENVIS_AO_CMD_DATA_H
#define IPCAMTENVIS_AO_CMD_DATA_H

#include <time.h>

#include "ag_defaults.h"

/* Alert constants: EventMonitor->Agent->SendFiles thread */
#define AC_ALERT_NAME   "alertName"
#define AC_ALERT_START  "startDate"
#define AC_ALERT_END    "endDate"
#define AC_ALERT_MSG    "path"

/*************************************************************************
 * Obsolete
 * All from and to Agent messages internal format: from Proxy, from Cam
 */
typedef enum {
    AO_CMD_NOTHING,             //No command
    AO_CMD_AGENT_CONNECT,       //Reconnect Agent to the cloud
    AO_CMD_AGENT_DISCONNECT,    //Disconnect Agent from the cloud

    AO_CMD_WS_START,            //(Re)Start Web Socket interface (restart included)
    AO_CMD_WS_STOP,             //Stop Web Socket interface
    AO_CMD_WS_PING,             //Answer to WS ping

    AO_CMD_RW_START,            //(Re)Start streaming NB! THese commands should came from WS and/or from the cloud
    AO_CMD_RW_STOP,             //Stop streaming

    AO_CMD_ASK_CAM,             //Request to camera
    AO_CMD_ANS_CAM,             //Response from camera

    AT_CMD_SIZE
} t_ao_agent_command;

/* Onsolete */
typedef enum {
    AO_ACT_NO,              /* No action expected */
    AO_ACT_ALIEN_PROPERTY   /* No shch a property in dB */
} t_ao_app_actions;

/* Message type */
typedef enum {
    AO_UNDEF,
    AO_IN_PROXY_ID,         /* Obsolete. Proxy device ID - the command feft for compatibility with M-3 agent*/
    AO_IN_CONNECTION_INFO,  /* Off line or on line */
    AO_IN_MANAGE_VIDEO,     /* Command to start video streaming received from the cloud */
    AO_WS_ANSWER,           /* Message from WS */
    AO_ALRT_CAM             /* camera alert */
} t_ao_msg_type;

/* WS messages type */
typedef enum {
    AO_WS_UNDEF,
    AO_WS_PING,
    AO_WS_ABOUT_STREAMING,
    AO_WS_ERROR
} t_ao_ws_msg_type;

/* Connection request data */
typedef struct {
    char url[4097];
    int port;
    char auth[129];   /* Session ID in out case */
} t_ao_conn;

/* AO_IN_CONNECTION_INFO */
typedef struct {
    t_ao_msg_type   msg_type;
    char            proxy_device_id[32];
    int             is_online;        /* 0 - ofline, 1 - online */
    char            proxy_auth[129];
    char            main_url[4097];
} t_ao_in_connection_state;

/* AO_IN_MANAGE_VIDEO */
typedef struct {
    t_ao_msg_type   msg_type;
    int start_it;               /* 0 if stop, 1 if start */
    int command_id;
} t_ao_in_manage_video;

/* AO_WS_ANSWER */
typedef struct {
    t_ao_msg_type       command_type;
    t_ao_ws_msg_type    ws_msg_type;    /* ERROR or ABOUT_STREAMING or NOT_INTERESTING */
    int                 is_start;       /* != 1 - do not analyze */
    int                 viewers_delta;  /* >0 - add viewers <0 - subtract viwers */
    int                 viwers_count;   /* viwers absolute amount. If negative - do not analyze */
    int                 rc;
} t_ao_ws_answer;

/* AO_WS_PING */
typedef struct {
    t_ao_msg_type       command_type;   /* AO_WS_ANSWER */
    t_ao_ws_msg_type    ws_msg_type;    /* AO_WS_PING */
    int                 timeout;       /* cloud pings period */
} t_ao_ws_ping;

/* All camera events list */
typedef enum {
    AC_CAM_EVENT_UNDEF,
    AC_CAM_START_MD, AC_CAM_STOP_MD, AC_CAM_START_SD, AC_CAM_STOP_SD, AC_CAM_START_IO, AC_CAM_STOP_IO,
    AC_CAM_MADE_SNAPSHOT, AC_CAM_RECORD_VIDEO,
    AC_CAM_STOP_SERVICE, AC_CAM_TIME_TO_PING,
    AC_CAM_GOT_FILE,
    AC_CAM_EVENTS_SIZE
} t_ac_cam_events;

/**
 * Convert string presentation to number
 *
 * @param string    - event name
 * @return  - event number or 0 (AC_CAM_EVENT_UNDEF) if the event name is not recognized
 */
t_ac_cam_events ac_cam_string2event(const char* string);

/**
 * Convert event number to event string
 *
 * @param event - event number
 * @return  - event name or "AC_CAM_EVENT_UNDEF" if event number is out of range
 */
const char* ac_cam_event2string(t_ac_cam_events event);

/**
 * Create alert message: got new file (snapshot/MD/SD) EventManager - Agent interface as
 * {"AC_ALERT_NAME": "AC_ALERT_MSG", "AC_CAM_GOT_FILE": "AC_ALERT_MSG": "path"}";
 *
 * @param path  - full path and file name
 * @param buf   - buffer to store aert
 * @param size  - bufer size
 * @return  - pointer to the buffer
 */
const char* ao_make_got_files(const char* path, char* buf, size_t size);

/**
 * Create alert message about MD or SD event Agent - EM interface
 *
 * @param event         - event number
 * @param start_date    - event start date; 0 if no start_date
 * @param end_date      - event end_date; 0 if no end date
 * @param buf           - buffer to store the result
 * @param size          - buffer size
 * @return  - pointer to the buffer
 */
const char* ao_make_MDSD(t_ac_cam_events event, time_t start_date, time_t end_date, char* buf, size_t size);


/* AO_ALRT_CAM */
typedef struct {
    t_ao_msg_type   command_type;   /* AO_ALRT_CAM */
    t_ac_cam_events cam_event;
    time_t start_date;              /* Alert start timestamp */
    time_t end_date;                /* Alert end timestamp */
    char buf[256];                  /* Message from EM event == AC_CAM_GOT_FILE*/
} t_ao_cam_alert;

typedef union {
    t_ao_msg_type               command_type;
    t_ao_in_connection_state    in_connection_state;
    t_ao_in_manage_video        in_manage_video;
    t_ao_ws_answer              ws_answer;
    t_ao_ws_ping                ws_ping;
    t_ao_cam_alert              cam_alert;
} t_ao_msg;

#endif /* IPCAMTENVIS_AO_CMD_DATA_H */