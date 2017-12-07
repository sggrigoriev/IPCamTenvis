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
 Created by gsg on 30/11/17.
 Local test for video proxing. VLC used as viewer
*/
#include <string.h>

#include "aq_queues.h"
#include "ag_settings.h"

#include "at_cam_video.h"

char host[500] = {0};
int port = -1;
char session_id[500] = {0};
char proxy_id[500] = {0};
char proxy_auth[500] = {0};

static void get_params(int argc, char* argv[]) {
    if(argc != 6) {
        printf("Usage: host port session_id proxy_id proxy_auth.");
        exit(-1);
    }
    unsigned int i;
    for(i = 1; i < argc; i++) {
        switch (i) {
            case 1: strncpy(host, argv[i], sizeof(host)-1); break;
            case 2: port = atoi(argv[i]); break;
            case 3: strncpy(session_id, argv[i], sizeof(session_id)-1); break;
            case 4: strncpy(proxy_id, argv[i], sizeof(proxy_id)-1); break;
            case 5: strncpy(proxy_auth, argv[i], sizeof(proxy_auth)-1); break;
            default:
                printf("Usage: host port session_id proxy_id proxy_auth.");
                exit(-1);
        }
    }
}

int main(int argc, char* argv[]) {

    get_params(argc, argv);

    if(!ag_load_config(ag_getCfgFileName())) exit(-1);    /* Run w/o input parameters */

    pu_start_logger(ag_getLogFileName(), ag_getLogRecordsAmt(), ag_getLogVevel());

    pu_log(LL_INFO, "Video test for video manager ");

    at_start_video_mgr(host, port, session_id, proxy_id, proxy_auth);

    pu_stop_logger();
    return 0;
}