/*
 *  Copyright 2018 People Power Company
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
 Created by gsg on 22/05/18.
 This is self-registrator for IPCam

 Algorithm:
    1. Find Cam's configuration file (proxyJSON.conf) if no file - Hard error (Or file creation)
    2. Register device. If Ok - test connection

  RCs list:
    0   success
    1   PROXY cfg file not found
    2   Error device id generation

  Test connection

*/

#include <stdlib.h>
#include <curl/curl.h>

#include "lib_http.h"
#include "pc_config.h"
#include "pu_logger.h"
#include "../../../../Presto_new/lib/proxy_eui64/eui64.h"  /* TODO!!! */

#include "OOBE_defaults.h"
#include "oobe_play_sounds.h"
#include "oobe_config.h"
#include "oobe_connectivity.h"

static int get_device_id() {
    if(!oobe_getProxyDeviceID()) {
        char eui_string[OOBE_DEVICE_ID_SIZE];
        char device_id[OOBE_DEVICE_ID_SIZE];
        if(!eui64_toString(eui_string, sizeof(eui_string))) {
            pu_log(LL_ERROR, "OOBE main: Unable to generate the Gateway DeviceID. Activaiton failed");
            return 0;
        }
        snprintf(device_id, OOBE_DEVICE_ID_SIZE-1, "%s%s", PROXY_DEVICE_ID_PREFIX, eui_string);
        if(!oobe_saveProxyDeviceID(device_id)) {
            pu_log(LL_WARNING, "OOBE main: Unable to store Device ID into configuration file");
        }
        pu_log(LL_INFO, "Proxy Device ID %s successfully generated", device_id);
    }
    return 1;
}

static void exiting(t_oobe_sound snd, int rc) {
    oobe_play_sound(snd);
    pu_stop_logger();
    curl_global_cleanup();
    exit(rc);
}
static void startup() {
    pu_start_logger(DEFAULT_OOBE_LOG_NAME, DEFAULT_OOBE_REC_AMT, DEFAULT_OOBE_LLEVEL);
    if(!oobe_load_config(DEFAULT_PROXY_CFG_FILE_NAME)) {
        pu_log(LL_ERROR, "%s: Error reading Proxy configuration file");
        exiting(OOBE_WHOOPS, OOBE_NO_PROXY_CFG);
    }
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        pu_log(LL_ERROR, "Error on cUrl initialiation. Exiting.");
        exiting(OOBE_WHOOPS, OOBE_SYSTEM_ERROR);
    }
}

int main() {

    startup();

/* Get Proxy Device ID */
    if(!get_device_id()) exiting(OOBE_WHOOPS, OOBE_DEV_ID_GENERATION_ERR);

    if(!oobe_getAuthToken() || !oobe_getMainCloudURL()) {
/* Have to register the device! */
        if(!oobe_getCloudParams(DEFAULT_OOBE_CAM_FILE_NAME)) exiting(OOBE_WHOOPS, OOBE_INVALID_CLOUD_PARAMS);
        if(!oobe_registerDevice() exiting(OOBE_WHOOPS, OOBE_INVALID_REGISTRATION);
    }
/* Test connection (endless loop in case of bad connectivity */
    if(!oobe_test_connection() exiting(OOBE_WHOOPS, OOBE_NO_CONNECTION);

    exiting(OOBE_BELL, OOBE_SUCCESS);
}