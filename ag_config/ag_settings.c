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

#include <assert.h>
#include <pthread.h>
#include <memory.h>

#include <cJSON.h>
#include "pu_logger.h"
#include "pc_config.h"
#include "pr_commands.h"

#include "ac_rtsp.h"
#include "au_string.h"
#include "ao_cma_cam.h"
#include "ag_defaults.h"
#include "ag_settings.h"

/************************************************************************
    Config file fields names
*/
#define AGENT_PROCESS_NAME          "AGENT_PROCESS_NAME"

#define AGENT_LOG_NAME              "LOG_NAME"
#define AGENT_LOG_REC_AMT           "LOG_REC_AMT"
#define AGENT_LOG_LEVEL             "LOG_LEVEL"
    #define AGENT_LL_DEBUG          "DEBUG"
    #define AGENT_LL_WARNING        "WARNING"
    #define AGENT_LL_INFO           "INFO"
    #define AGENT_LL_ERROR          "ERROR"

#define AGENT_QUEUES_REC_AMT        "QUEUES_REC_AMT"
#define AGENT_DEVICE_TYPE           "DEVICE_TYPE"

#define AGENT_PROXY_PORT            "PROXY_PORT"
#define AGENT_WUD_PORT              "WUD_PORT"
#define AGENT_WATCHDOG_TO_SEC       "WATCHDOG_TO_SEC"

#define AGENT_IPCAM_IP              "IPCAM_IP"
#define AGENT_IPCAM_PORT            "IPCAM_PORT"
#define AGENT_IPCAM_PROTOCOL        "IPCAM_PROTOCOL"
    #define AGENT_IC_RTMP           "RTMP"
    #define AGENT_IC_RTSP           "RTSP"
#define IPCAM_RESOLUTION            "IPCAM_RESOLUTION"
#define IR_LOW_RES                  "LO"
#define IR_HI_RES                   "HI"
#define AGENT_IPCAM_LOGIN           "IPCAM_LOGIN"
#define AGENT_IPCAM_PASSWORD        "IPCAM_PASSWORD"
#define AGENT_CHUNKS_AMOUNT         "CHUNKS_AMOUNT"
#define STREAMING_BUFFER_SIZE       "STREAMING_BUFFER_SIZE"

#define AGENT_CURLOPT_CAINFO        "CURLOPT_INFO"
#define AGENT_CURLOPT_SSL_VERIFYPEER "CURLOPT_SSL_VERIFYPEER"

/*************************************************************************
    Some macros
*/
#define AGS_ERR fprintf(stderr, "AGENT: Default value will be used instead\n")
#define AGS_RET(a,b) return (!initiated)?(a):(b)
/*************************************************************************
    Config values saved in memory
*/
static char         agent_process_name[PR_MAX_PROC_NAME_SIZE];
static char         agent_cfg_file_name[LIB_HTTP_MAX_URL_SIZE];

static char         log_name[LIB_HTTP_MAX_URL_SIZE];
static unsigned int log_rec_amt;
static log_level_t  log_level;

static unsigned int proxy_port;

static unsigned int queue_rec_amt;
static unsigned int agent_device_type;
static unsigned int wud_port;
static unsigned int watchdog_to_sec;

static char         ipcam_ip[LIB_HTTP_MAX_IPADDRES_SIZE];
static int          ipcam_port;
static int          video_protocol;
static unsigned int chunks_amount;
static unsigned int streaming_buffer_size;
static t_ao_cam_res ipcam_resolution;
static char         cam_login[129];
static char         cam_password[129];

static char         proxy_id[LIB_HTTP_DEVICE_ID_SIZE] = {0};
static char         proxy_auth_token[LIB_HTTP_AUTHENTICATION_STRING_SIZE] = {0};
static char         main_url[LIB_HTTP_MAX_URL_SIZE] = {0};

static int          curlopt_ssl_verify_peer = 1;
static char         curlopt_ca_info[LIB_HTTP_MAX_URL_SIZE] = {0};

static char         conf_fname[LIB_HTTP_MAX_URL_SIZE] = {0};
/***********************************************************************
    Local functions
*/
/* Settings guard. Used in all "thread-protected" functions */
static pthread_mutex_t local_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Indicates are defaults set 1 if set 0 if not */
static int initiated = 0;
/* Initiates config variables by defult values */
static void initiate_defaults();
/* Copy log_level_t value (see pu_logger.h) of field_name of cgf object into uint_setting. */
/* Copy default value in case of absence of the field in the object */
static void getLLTValue(cJSON* cfg, const char* field_name, log_level_t* llt_setting);
/*******************************************************************************
 * Get configured video protocol type
 * @param cfg - parsed configuration file
 * @param field_name - setting name
 * @param prot_val - setting value converted to int (see ad_defaults.h)
 */
static void getProtocolValue(cJSON* cfg, const char* field_name, int* prot_val);
static void getCamResValue(cJSON* cfg, const char* field_name, t_ao_cam_res* cam_res);

/*************************************************************************
    Set of "get" functions to make an access to settings for Presto modules
*/
const char* ag_getCfgFileName() {
    AGS_RET(DEFAULT_CFG_FILE_NAME, conf_fname);
}
const char* ag_getAgentName() {
    AGS_RET(DEFAULT_AGENT_NAME, agent_process_name);
}
/*****************************************************
    Logger settings
*/
const char*     ag_getLogFileName() {
    AGS_RET(DEFAULT_LOG_NAME, log_name);
}
size_t          ag_getLogRecordsAmt() {
    AGS_RET(DEFAULT_LOG_RECORDS_AMT, log_rec_amt);
}
log_level_t     ag_getLogVevel() {
    AGS_RET(DEFAULT_LOG_LEVEL, log_level);
}
/*****************************************************
    Agent internal & external communication settings
*/
unsigned int    ag_getProxyPort() {
    AGS_RET(DEFAULT_PROXY_PORT, proxy_port);
}
size_t          ag_getQueuesRecAmt(){
    AGS_RET(DEFAULT_QUEUES_REC_AMOUNT, queue_rec_amt);
}
unsigned int    ag_getAgentDeviceType() {
    AGS_RET(DEFAULT_AGENT_DEVICE_TYPE, agent_device_type);
}
unsigned int    ag_getWUDPort() {
    AGS_RET(DEFAULT_WUD_PORT, wud_port);
}
unsigned int    ag_getAgentWDTO() {
    AGS_RET(DEFAULT_WATCHDOG_TO_SEC, watchdog_to_sec);
}

const char*     ag_getCamIP() {
    AGS_RET(DEFAULT_IPCAM_IP, ipcam_ip);
}
int             ag_getCamPort() {
    AGS_RET(DEFAULT_IPCAM_PORT, ipcam_port);
}
int             ag_getIPCamProtocol() {
    AGS_RET(DEFAULT_VIDEO_PROTOCOL, video_protocol);
}
unsigned int    ag_getVideoChunksAmount() {
    AGS_RET(DEFAULT_CHUNKS_AMOUNT, chunks_amount);
}
t_ao_cam_res    ag_getCamResolution() {
    AGS_RET(DEFAULT_IPCAM_RESOLUTION, ipcam_resolution);
}
unsigned int    ag_getStreamBufferSize() {
    AGS_RET(DEFAULT_MAX_UDP_STREAM_BUFF_SIZE, streaming_buffer_size);
}
const char*     ag_getCamLogin() {
    AGS_RET(DEFAULT_IPCAM_LOGIN, cam_login);
}
const char*     ag_getCamPassword() {
    AGS_RET(DEFAULT_IPCAM_PASSWORD, cam_password);
}

const char* ag_getCurloptCAInfo() {
    return curlopt_ca_info;
}
int ag_getCurloptSSLVerifyPeer() {
    return curlopt_ssl_verify_peer;
}

void ag_saveProxyAuthToken(const char* token) {
    if(token) au_strcpy(proxy_auth_token, token, sizeof(proxy_auth_token));
}
const char* ag_getProxyAuthToken() {
    return proxy_auth_token;
}

void ag_saveProxyID(const char* proxyID) {
    au_strcpy(proxy_id, proxyID, sizeof(proxy_id));
}
const char* ag_getProxyID() {
    return proxy_id;
}

void ag_saveMainURL(const char* mu) {
    au_strcpy(main_url, mu, sizeof(main_url));
}
const char* ag_getMainURL() {
    return main_url;
}

int ag_load_config(const char* cfg_file_name) {
    cJSON* cfg = NULL;
    assert(cfg_file_name);

    pthread_mutex_lock(&local_mutex);

    strcpy(conf_fname, cfg_file_name);

    initiate_defaults();
    if(cfg = load_file(cfg_file_name), cfg == NULL) {
        pthread_mutex_unlock(&local_mutex);
        return 0;
    }
/* Now load data */
    if(!getStrValue(cfg, AGENT_LOG_NAME, log_name, sizeof(log_name)))                           AGS_ERR;
    if(!getUintValue(cfg, AGENT_LOG_REC_AMT, &log_rec_amt))                                     AGS_ERR;
    getLLTValue(cfg, AGENT_LOG_LEVEL, &log_level);

     if(!getUintValue(cfg, AGENT_DEVICE_TYPE, &agent_device_type))                              AGS_ERR;

    if(!getUintValue(cfg, AGENT_QUEUES_REC_AMT, &queue_rec_amt))                                AGS_ERR;
    if(!getUintValue(cfg, AGENT_PROXY_PORT, &proxy_port))                                       AGS_ERR;
    if(!getUintValue(cfg, AGENT_WUD_PORT, &wud_port))                                           AGS_ERR;
    if(!getStrValue(cfg, AGENT_PROCESS_NAME, agent_process_name, sizeof(agent_process_name)))   AGS_ERR;
    if(!getUintValue(cfg, AGENT_WATCHDOG_TO_SEC, &watchdog_to_sec))                             AGS_ERR;

    if(!getStrValue(cfg, AGENT_IPCAM_IP, ipcam_ip, sizeof(ipcam_ip)))                           AGS_ERR;
    if(!getUintValue(cfg, AGENT_IPCAM_PORT, (unsigned int *)&ipcam_port))                       AGS_ERR;

    getProtocolValue(cfg, AGENT_IPCAM_PROTOCOL, &video_protocol);
    getCamResValue(cfg, IPCAM_RESOLUTION, &ipcam_resolution);

    if(!getUintValue(cfg, STREAMING_BUFFER_SIZE, &streaming_buffer_size))                       AGS_ERR;

    if(!getUintValue(cfg, AGENT_CHUNKS_AMOUNT, &chunks_amount))                                 AGS_ERR;
    if(!getStrValue(cfg, AGENT_IPCAM_LOGIN, cam_login, sizeof(cam_login)))                      AGS_ERR;
    if(!getStrValue(cfg, AGENT_IPCAM_PASSWORD, cam_password, sizeof(cam_password)))             AGS_ERR;

    if(!getUintValue(cfg, AGENT_CURLOPT_SSL_VERIFYPEER, (unsigned int* )&curlopt_ssl_verify_peer)) AGS_ERR;
    if(!getStrValue(cfg, AGENT_CURLOPT_CAINFO, curlopt_ca_info, sizeof(curlopt_ca_info)))       AGS_ERR;

    cJSON_Delete(cfg);

    pthread_mutex_unlock(&local_mutex);
    return 1;

}
/************************************************************************************************
 * Local functions implementation
 */
static void initiate_defaults() {
    initiated = 1;

    au_strcpy(agent_process_name, DEFAULT_AGENT_NAME, PR_MAX_PROC_NAME_SIZE);
    au_strcpy(agent_cfg_file_name, DEFAULT_CFG_FILE_NAME, LIB_HTTP_MAX_URL_SIZE);

    au_strcpy(log_name, DEFAULT_LOG_NAME, LIB_HTTP_MAX_URL_SIZE);
    log_rec_amt = DEFAULT_LOG_RECORDS_AMT;
    log_level = DEFAULT_LOG_LEVEL;

    proxy_port = DEFAULT_PROXY_PORT;
    queue_rec_amt = DEFAULT_QUEUES_REC_AMOUNT;
    agent_device_type = DEFAULT_AGENT_DEVICE_TYPE;

    wud_port = DEFAULT_WUD_PORT;
    watchdog_to_sec = DEFAULT_WATCHDOG_TO_SEC;

    ipcam_port = DEFAULT_IPCAM_PORT;
    au_strcpy(ipcam_ip, DEFAULT_IPCAM_IP, LIB_HTTP_MAX_IPADDRES_SIZE);
    chunks_amount = DEFAULT_CHUNKS_AMOUNT;
    streaming_buffer_size = DEFAULT_MAX_UDP_STREAM_BUFF_SIZE;
    ipcam_resolution = DEFAULT_IPCAM_RESOLUTION;

    au_strcpy(cam_login, DEFAULT_IPCAM_LOGIN, sizeof(cam_login)-1);
    au_strcpy(cam_password, DEFAULT_IPCAM_PASSWORD, sizeof(cam_password)-1);

    curlopt_ssl_verify_peer = DEFAULT_IPCAM_SSL_VERIFY_PEER;
    curlopt_ca_info[0] = '\0';
}

/*
 Get log_level_t value
    cfg           - poiner to cJSON object containgng configuration
    field_name    - JSON fileld name
    llt_setting   - returned value of field_name
*/
static void getLLTValue(cJSON* cfg, const char* field_name, log_level_t* llt_setting) {
    char buf[10];
    if(!getStrValue(cfg, field_name, buf, sizeof(buf)))
        fprintf(stderr, "Default will be used instead.\n");
    else if(!strcmp(buf, AGENT_LL_DEBUG))
        *llt_setting = LL_DEBUG;
    else if(!strcmp(buf, AGENT_LL_WARNING))
        *llt_setting = LL_WARNING;
    else if(!strcmp(buf, AGENT_LL_INFO))
        *llt_setting = LL_INFO;
    else if(!strcmp(buf, AGENT_LL_ERROR))
        *llt_setting = LL_ERROR;
    else
        fprintf(stderr, "Setting %s = %s. Posssible values are %s, %s, %s or %s. Default will be used instead\n",
                field_name, buf, AGENT_LL_DEBUG, AGENT_LL_INFO,  AGENT_LL_WARNING, AGENT_LL_ERROR
        );
}
static void getProtocolValue(cJSON* cfg, const char* field_name, int* prot_val) {
    char buf[10];
    if(!getStrValue(cfg, field_name, buf, sizeof(buf)))
        fprintf(stderr, "Default will be used instead.\n");
    else if(!strcmp(buf, AGENT_IC_RTMP))
        *prot_val = AG_VIDEO_RTMP;
    else if(!strcmp(buf, AGENT_IC_RTSP))
        *prot_val = AG_VIDEO_RTSP;
    else
        fprintf(stderr, "Setting %s = %s. Posssible values are %s or %s. Default will be used instead\n",
                field_name, buf, AGENT_IC_RTMP, AGENT_IC_RTSP
        );
}
static void getCamResValue(cJSON* cfg, const char* field_name, t_ao_cam_res* cam_res) {
    char buf[10];
    if(!getStrValue(cfg, field_name, buf, sizeof(buf)))
        fprintf(stderr, "Default will be used instead.\n");
    else if(!strcmp(buf, IR_LOW_RES))
        *cam_res = AO_RES_LO;
    else if(!strcmp(buf, IR_HI_RES))
        *cam_res = AO_RES_HI;
    else
        fprintf(stderr, "Setting %s = %s. Posssible values are %s, %s, %s or %s. Default will be used instead\n",
                field_name, buf, AGENT_LL_DEBUG, AGENT_LL_INFO,  AGENT_LL_WARNING, AGENT_LL_ERROR
        );

}