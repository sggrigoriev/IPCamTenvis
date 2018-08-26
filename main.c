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
#include <stdint.h>
#include <getopt.h>

#include <curl/curl.h>  /* for global init/deinit */
#include <gst/gst.h>    /* for gloal init/deinit */


#include "pu_logger.h"

#include "ag_settings.h"
#include "agent_release_version.h"
#include "at_main_thread.h"

/* Cutle & Gstreamer initiations */
static int gst_and_curl_startup() {
    CURLcode res = CURLE_OK;

    if (res = curl_global_init(CURL_GLOBAL_ALL), res != CURLE_OK) {
        pu_log(LL_ERROR, "%s: Error in curl_global_init. RC = %d", __FUNCTION__, res);
        goto on_error;
    }
    GError *err = NULL;
    if (!gst_init_check(NULL, NULL, &err) || err != NULL) {
        pu_log(LL_ERROR, "%s: Error GST init: %s", __FUNCTION__, err->message);
        g_error_free(err);
        goto on_error;
    }
    return 1;
on_error:
    curl_global_cleanup();
    gst_deinit();
    return 0;
}

static void print_Agent_start_params() {
    pu_log(LL_INFO, "Agent start parameters:");
    pu_log(LL_INFO, "\tConfiguration file name: %s", ag_getCfgFileName());

    pu_log(LL_INFO, "\tAgent process name: %s", ag_getAgentName());

    pu_log(LL_INFO, "\tLog file name: %s", ag_getLogFileName());
    pu_log(LL_INFO, "\t\tLog records amount: %d", ag_getLogRecordsAmt());
    pu_log(LL_INFO, "\t\tLog level: %d", ag_getLogVevel());

    pu_log(LL_INFO, "\tMax records in queue: %d", ag_getQueuesRecAmt());

    pu_log(LL_INFO, "\tAgent-Proxy communication port: %d", ag_getProxyPort());
    pu_log(LL_INFO, "\tAgent-WUD communication port: %d", ag_getWUDPort());
    pu_log(LL_INFO, "\tWatchdog TO sec: %d", ag_getAgentWDTO());

    pu_log(LL_INFO, "\tAgent Device Type: %d", ag_getAgentDeviceType());

    pu_log(LL_INFO, "\tCam IP: %s", ag_getCamIP());
    pu_log(LL_INFO, "\tCam port: %d", ag_getCamPort());
    pu_log(LL_INFO, "\tURL postfix: %s", ag_getCamPostfix());
    pu_log(LL_INFO, "\tCanm Mode: %s", ag_getCamMode());
    pu_log(LL_INFO, "\tCam Channel: %s", ag_getCamChannel());
    pu_log(LL_INFO, "\tCam Login: %s", ag_getCamLogin());
    pu_log(LL_INFO, "\tCam Password: %s", ag_getCamPassword());
    pu_log(LL_INFO, "\tCam Protocol: %d", ag_getIPCamProtocol());
    pu_log(LL_INFO, "\tCam streaming: interleaved mode = %d", ag_isCamInterleavedMode());

    pu_log(LL_INFO, "\tStreaming buffer size: %d", ag_getStreamBufferSize());

    pu_log(LL_INFO, "\tCurlopt SSL Verify Peer: %d", ag_getCurloptSSLVerifyPeer());
    pu_log(LL_INFO, "\tCurlopt CA Info: %s", ag_getCurloptCAInfo());
}

/*
 * Debugging utility
 */
volatile uint32_t contextId = 0;
void signalHandler( int signum ) {
    pu_log(LL_ERROR, "TENVIS.%s: Interrupt signal (%d) received. ContextId=%d thread_id=%d\n", __FUNCTION__, signum, contextId, pthread_self());
    exit(signum);
}


int main(int argc, char* argv[]) {
    signal(SIGSEGV, signalHandler);
    signal(SIGBUS, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGKILL, signalHandler);

    char* config = DEFAULT_CFG_FILE_NAME;

    if(argc > 1) {
        int c = getopt(argc, argv, "p:");
        if(c != 'p') {
            fprintf(stderr, "Wrong start parameter. Only -p<config_file_path_and_name> allowed");
            exit(-1);
        }
        config = optarg;
    }

    printf("Tenvis v %s\n", AGENT_FIRMWARE_VERSION);

    if(!ag_load_config(config)) exit(-1);

    pu_start_logger(ag_getLogFileName(), ag_getLogRecordsAmt(), ag_getLogVevel());
    print_Agent_start_params();

    if(!gst_and_curl_startup()) {
        pu_log(LL_ERROR, "Curl/Gstreamer init error. Exiting\n");
    }
    else {
        at_main_thread();   /*Main Agent'd work cycle is hear */
    }

    pu_stop_logger();

    curl_global_cleanup();
    gst_deinit();
    exit(0);
}

