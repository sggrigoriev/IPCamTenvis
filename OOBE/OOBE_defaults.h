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
 Created by gsg on 12/06/18.
*/

#ifndef IPCAMTENVIS_OOBE_DEFAULTS_H
#define IPCAMTENVIS_OOBE_DEFAULTS_H

#define DEFAULT_OOBE_CAM_FILE_NAME      "/tmpfs/qrinfo"
#define DEFAULT_PROXY_CFG_FILE_NAME     "./proxyJSON.conf"    /* Default Proxy configuration file */
#define DEFAULT_MAIN_URL_FILE_NAME      "./cloud_url"
#define DEFAULT_AUTH_TOKEN_FILE_NAME    "./auth_token"

#define PROXY_DEVICE_ID_PREFIX      "IPCam-"

#define DEFAULT_OOBE_LOG_NAME        "./OOBE_LOG"           /* OOBE log file name. Configurable */
#define DEFAULT_OOBE_REC_AMT        1000
#define DEFAULT_OOBE_LLEVEL         LL_ERROR

#define OOBE_SUCCESS                0
#define OOBE_NO_PROXY_CFG           1
#define OOBE_DEV_ID_GENERATION_ERR  2
#define OOBE_INVALID_CLOUD_PARAMS   3
#define OOBE_INVALID_REGISTRATION   4
#define OOBE_NO_CONNECTION          5
#define OOBE_SYSTEM_ERROR           100

#define OOBE_ACTIVATION_KEY_LEN     129
#define OOBE_DEVICE_ID_SIZE         LIB_HTTP_DEVICE_ID_SIZE                 /* Max len of defice id */
#define OOBE_MAX_URL_LEN            LIB_HTTP_MAX_URL_SIZE

#define OOBE_HTTP_RETRIES_TO_SEC    3

#define OOBE_HTTPS                  "https://"
#define OOBE_HTTP                   "http://"
#define OOBE_REGISTRATOIN_FMT       "https://%s/cloud/json/devices?deviceId=%s&deviceType=7000&authToken=true"

#define OOBE_HD_ACTIVATION_KEY_FMT  "ACTIVATION_KEY:%s"
#define OOBE_HD_CONTENT_TYPE        "Content-Type:application/json"


#endif /* IPCAMTENVIS_OOBE_DEFAULTS_H */
