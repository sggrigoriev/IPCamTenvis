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

#include <stdio.h>

#include "ag_settings.h"
#include "at_video_connector.h"

int main() {
    printf("Video test for video connector (VLC is used as client\n");

    if(!ag_load_config(ag_getCfgFileName())) exit(-1);    /* Run w/o input parameters */

    pu_start_logger(ag_getLogFileName(), ag_getLogRecordsAmt(), ag_getLogVevel());

    void* par= NULL;
    vc_thread(par);   /*Main Agent'd work cycle is hear */

    pu_stop_logger();
    exit(0);
}