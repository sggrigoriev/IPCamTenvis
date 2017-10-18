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

const char*     ag_getIPCamIP();            /* IP Camera address */
/**************************************************************************************************************************
    Thread-protected functions
*/
/* Initiate the configuration service. Load data from configuration port; Initiates default values
   Return 1 if Ok, 0 if not
*/
int ag_load_config(const char* cfg_file_name);


#endif /* IPCAMTENVIS_AG_SETTINGS_H */
