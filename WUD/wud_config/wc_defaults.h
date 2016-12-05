//
// Created by gsg on 29/11/16.
//

#ifndef PRESTO_WC_DEFAULTS_H
#define PRESTO_WC_DEFAULTS_H

#define WC_MAX_PATH     4097
#define WC_MAX_PROCESS_NAME_LEN 30
#define WC_MAX_MSG_LEN  1000

#define WD_DEFAULT_CFG_FILE_NAME   "/media/sf_GWDrive/Presto_new/WUD/wud.conf"

//Common defaults
#define WC_DEFAULT_LOG_NAME        "/media/sf_GWDrive/Presto_new/build/WUD_LOG"
#define WC_DEFAULT_LOG_RECORDS_AMT 5000
#define WC_DEFAULT_LOG_LEVEL       LL_DEBUG

#define WC_DEFAULT_QUEUES_REC_AMT   1024

#define WC_DEFAULT_PID_DIRECTORY    "/var/run/"
#define WC_DEFAULT_WUD_NAME         "wud"
#define WC_DEFAULT_PIDF_EXTENCION   "pid"
#define WC_PROC_DIR                 "/proc/"

#define WUD_DEFAULT_WORKING_DIRECTORY    "./"
#define WUD_DEFAULT_COMM_PORT            8887
#define WUD_DEFAULT_CHILDREN_SHUTDOWN_TO_SEC    10

#define WUD_DEFAULT_FW_DOWNLOAD_FOLDER       "./download"
#define WUD_DEFAULT_FW_UPGRADE_FOLDER        "./upgrade"

#define WUD_DEFAULT_AGENT_PROCESS_NAME       "Agent"
#define WUD_DEFAULT_AGENT_BINARY_NAME        "agent"
#define WUD_DEFAULT_AGENT_RUN_PARAMETERS     NULL
#define WUD_DEFAULT_AGENT_WORKING_DIRECTORY  "./agent_wd"
#define WUD_DEFAULT_AGENT_WD_TIMEOUT_SEC      10                 //agent's own timeout to send he DW should be less

#define WUD_DEFAULT_PROXY_PROCESS_NAME       "Proxy"
#define WUD_DEFAULT_PROXY_BINARY_NAME        "proxy"
#define WUD_DEFAULT_PROXY_RUN_PARAMETERS     NULL
#define WUD_DEFAULT_PROXY_WORKING_DIRECTORY  "./proxy_wd"
#define WUD_DEFAULT_PROXY_WD_TIMEOUT_SEC      10                 //proxy's own timeout to send he DW should be less


#endif //PRESTO_WC_DEFAULTS_H
