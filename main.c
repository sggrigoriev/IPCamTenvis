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

    IPCam's Agent process main function
*/
#include <stdio.h>
#include <stdlib.h>

#include "pu_logger.h"

#include "ag_settings.h"
#include "agent_release_version.h"
#include "at_main_thread.h"

/* Help for Agent start parameters syntax */
static void print_Agent_start_params();

int main() {
    printf("Presto v %s\n", AGENT_FIRMWARE_VERSION);

    if(!ag_load_config(ag_getCfgFileName())) exit(-1);    /* Run w/o input parameters */

    pu_start_logger(ag_getLogFileName(), ag_getLogRecordsAmt(), ag_getLogVevel());
    print_Agent_start_params();

    at_main_thread();   /*Main Agent'd work cycle is hear */

    pu_stop_logger();
    exit(0);
}

static void print_Agent_start_params() {
    pu_log(LL_INFO, "Agent start parameters:");
    pu_log(LL_INFO, "\tConfiguration file name: %s", ag_getCfgFileName());
    pu_log(LL_INFO, "\tLog file name: %s", ag_getLogFileName());
    pu_log(LL_INFO, "\tMax records in queue: %d", ag_getQueuesRecAmt());
    pu_log(LL_INFO, "\t\tRecords amount in log file: %d", ag_getLogRecordsAmt());
    pu_log(LL_INFO, "\t\tLog level: %d", ag_getLogVevel());
    pu_log(LL_INFO, "\tAgent-Proxy communication port: %d", ag_getProxyPort());
    pu_log(LL_INFO, "\tAgent-WUD communication port: %d", ag_getWUDPort());
    pu_log(LL_INFO, "\tAgent name: %s", ag_getAgentName());
    pu_log(LL_INFO, "\tAgent watchdog sending interval in seconds: %d", ag_getAgentWDTO());
    pu_log(LL_INFO, "\tAgent device type: %d", ag_getAgentDeviceType());
    pu_log(LL_INFO, "\tCam IP: %s", ag_getIPCamIP());
    pu_log(LL_INFO, "\tStreaming protocol: %d", ag_getIPCamProtocol());
    pu_log(LL_INFO, "\tBuffer chunks amount: %d", ag_getVidoeChunksAmount());
}