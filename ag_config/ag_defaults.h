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
 Created by gsg on 16/10/17.

 Contains defaults for the Tenvis Agent
*/

#ifndef IPCAMTENVIS_AG_DEFAULTS_H
#define IPCAMTENVIS_AG_DEFAULTS_H

#include "lib_http.h"
#include "pr_commands.h"

#define DEFAULT_CFG_FILE_NAME       "./Tenvis.conf"

#define DEFAULT_AGENT_NAME          "Agent"
#define DEFAULT_QUEUES_REC_AMOUNT   1024
#define DEFAULT_AGENT_DEVICE_TYPE   7000

/* Logger defaults */
#define DEFAULT_LOG_NAME        "./TENVIS_LOG"      /* Agent log file name. Configurable */
#define DEFAULT_LOG_RECORDS_AMT 5000                /* Max records amount in the log. Configurable */
#define DEFAULT_LOG_LEVEL       LL_ERROR            /* Log level. Configurable. */

/* Proxy communiation defaults */
#define DEFAULT_PROXY_PORT      8888

/* WUD communication defaults */
#define DEFAULT_WUD_PORT        8887                /* Port to communicate with WUD. Configured */
#define DEFAULT_WATCHDOG_TO_SEC 30                  /* Default time for WD sending to WUD */

/* IP Cam settings */
#define DEFAULT_IPCAM_IP        "127.0.0.1"
#define DEFAULT_IPCAM_PORT      554
#define IPCAM_HIRES             "11"
#define IPCAP_LORES             "12"
#define AG_VIDEO_RTMP           1
#define AG_VIDEO_RTSP           0
#define DEFAULT_VIDEO_PROTOCOL  AG_VIDEO_RTSP
#define DEFAULT_CHUNKS_AMOUNT   12
#define DEFAULT_MAX_UDP_STREAM_BUFF_SIZE    8192    /* I had 1514 in trace and add a little...*/
#define DEFAULT_IPCAM_RESOLUTION            AO_RES_HI
#define DEFAULT_IPCAM_LOGIN     ""
#define DEFAULT_IPCAM_PASSWORD  ""
/**********************************************************
 * Local test settings
 */
#define DEFAULT_LOCAL_AGENT_PORT    8889
/************************************************************
    Non-configurable defaults
*/
#define DEFAULT_PROXY_WRITE_THREAD_TO_SEC   1   /* Timeout for incoming events in agent read/write threas. Zero means no timeout */
#define DEFAULT_CAM_WRITE_THREAD_TO_SEC     3600 /* Timeout for incoming events in cam_write therad */
#define DEFAULT_INTERNET_RECONNECT_ATTEMPTS 3   /* Times to repeat connection attempts to video server */
#define DEFAULT_INTERNET_RECONECT_TIMEOUTS  10  /* TO to start new reconnect attempt */

#define DEFAULT_CAM_RSTP_SESSION_ID_LEN  20

#define DEFAULT_PPC_VIDEO_FOLDER        "ppcvideoserver"

#define DEFAULT_WC_START_PLAY           "START"
#define DEFAULT_WC_STOP_PLAY            "STOP"
#define DEFAULT_CLOUD_CONN_STRING       "https://sbox.presencepro.com/cloud/json/settingsServer/streaming"

#define DEFAULT_IPCAM_SSL_VERIFYER      1

#define AG_DBG  pu_log(LL_DEBUG, "%s: %d", __FUNCTION__, __LINE__)

#endif /* IPCAMTENVIS_AG_DEFAULTS_H */
