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
#define DEFAULT_IPCAM_IP        "192.168.100.14"

/************************************************************
    Non-configurable defaults
*/
#define DEFAULT_PROXY_WRITE_THREAD_TO_SEC   1   /* Timeout for incoming events in agent read/write threas. Zero means no timeout */
#define DEFAULT_CAM_WRITE_THREAD_TO_SEC     3600 /* Timeout for incoming events in cam_write therad */

#endif /* IPCAMTENVIS_AG_DEFAULTS_H */
