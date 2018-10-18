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

#define AG_VIDEO_RTMP           1
#define AG_VIDEO_RTSP           0
#define DEFAULT_VIDEO_PROTOCOL  AG_VIDEO_RTSP
#define DEFAULT_MAX_UDP_STREAM_BUFF_SIZE    8192    /* I had 1514 in trace and add a little...*/

#define DEFAULT_IPCAM_LOGIN     "admin"
#define DEFAULT_IPCAM_PASSWORD  "admin"

#define DEFAULT_IPCAM_INTERLEAVED_MODE  1

#define DEFAULT_PPC_VIDEO_FOLDER        "ppcvideoserver"

#define DEFAULT_IS_SSL                     1

#define DEFAULT_IPCAM_SSL_VERIFY_PEER      0

/* Local & non-configurable defaults */
#define DEFAULT_CLOUD_PING_TO   300

#define DEFAULT_DT_FILES_PATH   "/mnt/rd/0"
#define DEFAULT_DT_FILES_PREFIX "0-"            /*NB!!! ag_cam_io/ac_cam functions strictly relates on PREFIX & POSTFIX length!!! */
#define DEFAULT_MD_FILE_POSTFIX "M"
#define DEFAULT_SD_FILE_POSTFIX "S"
#define DEFAULT_SNAP_FILE_POSTFIX "P"

#define DEFAULT_MSD_FILE_EXT    "mp4"
#define DEFAULT_SNAP_FILE_EXT    "jpg"

#define DEFAULT_AM_READ_TO_SEC  1
#define DEFAULT_AM_ALERT_TO_SEC 10
#define DEFAULT_AV_ASK_TO_SEC   5*60

#define DEFAULT_DB_PATH     "./ipcam_db_stor.txt"
#define DEFAULT_SNAP_PATH   "./the_snapshot.jpg"

#endif /* IPCAMTENVIS_AG_DEFAULTS_H */
