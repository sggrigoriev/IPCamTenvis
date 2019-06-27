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
 Getters and setters for Camera configuration file
*/

#ifndef IPCAMTENVIS_AG_SETTINGS_H
#define IPCAMTENVIS_AG_SETTINGS_H

#include <stddef.h>     /* For size_t */

#include "pu_logger.h"

#include "ao_cma_cam.h"
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
const char*     ag_getCamPostfix();         /* Some shit right after the ip:port/ */
const char*     ag_getCamChannel();         /* "0" - hi res, "1" - low res, "2" - 2nd channel - applicable for av & video */
const char*     ag_getCamMode();            /* "av" or "video" or "audio" */
const char*     ag_getCamLogin();
const char*     ag_getCamPassword();
int             ag_getIPCamProtocol();      /* RTSP only!*/
int             ag_isCamInterleavedMode();  /* Should be 1 */

unsigned int    ag_getStreamBufferSize();


long    ag_getConnectRespTO();      /* TO to wait video connect confirmation from the cloud */
long    ag_getDisconnectRespTO();   /* TO to wait video disconnect confirmation from the cloud */
long    ag_getSessionIdTO();        /* TO to wait session info form cloud */

/* Thread-protected functions */
void ag_saveProxyID(const char* proxyID);       /* Save deviceID sent by Proxy */
const char* ag_getProxyID();                    /* Get device ID */

void ag_saveProxyAuthToken(const char* token);  /* Save auth token sent by Proxy */
const char* ag_getProxyAuthToken();             /* Get auth token */

void ag_saveMainURL(const char* mu);            /* Save main URL sent by Proxy */
const char* ag_getMainURL();                    /* Get main URL */
/********************************/

int ag_getIsSSL();                              /* 1 if SSL (HTTPS) used */

const char* ag_getCurloptCAInfo();              /* cURL-related stuff */
int ag_getCurloptSSLVerifyPeer();               /* cURL-related stuff */

int ag_load_config(const char* cfg_file_name);  /* Load configuration file */

int ag_load_cam_settings();                     /* Not in use. Load cam settings from file. Return 0 if no file or file corrupted */
const char* ag_request_cam_settings();          /* Not in use. Return JSON with settings required or NULL if error */
const char* ag_get_cam_properties(const char* property_name); /* Not in use. Return JSON with camera propertiy and format. If property_name == NULL - returns all. NULL if error*/
/* Alert reactions */
int ag_isMsgSendOnAlert(t_ac_cam_events alert_type);    /* Not in use */
int ag_isFileSendOnAlert(t_ac_cam_events alert_type);   /* Not in use */

#endif /* IPCAMTENVIS_AG_SETTINGS_H */