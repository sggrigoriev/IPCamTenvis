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
*/

#ifndef IPCAMTENVIS_AG_SETTINGS_H
#define IPCAMTENVIS_AG_SETTINGS_H

#include <stddef.h>     /* For size_t */

#include "pu_logger.h"

#include "ao_cmd_data.h"

/*
    Set of "get" functions to make an access to settings for Presto modules
*/
const char* ag_getCfgFileName();            /* Return Agent's config path+filename */
const char* ag_getAgentName();              /* Return process name */
/*****************************************************
    Logger settings
*/
const char*     ag_getLogFileName();        /* Return LOG-file name */
size_t          ag_getLogRecordsAmt();      /* Return max capacity of log file */
log_level_t     ag_getLogVevel();           /*  Return the min log level to be stored in LOG_FILE */

/*****************************************************
    Agent internal & external communication settings
*/
unsigned int    ag_getProxyPort();          /* Return the port# for communications with the Proxy */
size_t          ag_getQueuesRecAmt();       /* Return max amount of records kept in Agent's queues */
unsigned int    ag_getAgentDeviceType();    /* Return GW/Agent "main" device type. For camera it is the only device type */

unsigned int    ag_getWUDPort();            /* Return WUD communication port */
unsigned int    ag_getAgentWDTO();          /* timeout for watchdog sending */

const char*     ag_getCamIP();            /* IP Camera address */
int             ag_getCamPort();            /* IP Cam port */
int             ag_getIPCamProtocol();      /* RTMP or RTSP */
unsigned int    ag_getVideoChunksAmount();  /* Amount of buffers for video translation */

time_t    ag_getSessionIdTO();        /* TO to wait session info form cloud */
time_t    ag_getConnectRespTO();      /* TO to wait video connect confirmation from the cloud */
time_t    ag_getDisconnectRespTO();   /* TO to wait video disconnect confirmation from the cloud */
/**************************************************************************************************************************
    Thread-protected functions
*/
/*****************************************
 * @return NULL if no string or connection string
 */
const t_ao_in_video_params ag_getVideoConnectionData();
/*****************************************
 * Erase connection data to NULL
 */
void ag_dropVideoConnectionData();
/******************************************
 * Save cloud connection parameters
 * @param con_data - pointer to cloud connection parameters to be saved
 */
void ag_saveVideoConnectionData(t_ao_in_video_params con_data);
/*********************************************************************
 * Save stream session sent by cloud
 * @param stream_details
 */
void ag_saveStreamDetails(t_ao_in_stream_sess_details stream_details);
void ag_dropStreamDetails();

/* Initiate the configuration service. Load data from configuration port; Initiates default values
   Return 1 if Ok, 0 if not
*/
void ag_saveProxyID(const char* proxy_id);
const char* ag_getProxyID();

void ag_saveClientIP(const char* ip_address);
const char* ag_getClientIP(char* buf, size_t size);

void ag_saveClientPort(int port);
int ag_getClientPort();

void ag_saveServerPort(int port);
int ag_getServerPort();

int ag_load_config(const char* cfg_file_name);


#endif /* IPCAMTENVIS_AG_SETTINGS_H */
