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
 Created by gsg on 25/02/18.
*/
#include <au_string/au_string.h>
#include <ag_config/ag_settings.h>
#include <string.h>
#include "ac_cam.h"
#include "ac_http.h"

/*
 *  GET http://192.168.1.58/cgi-bin/hi3510/setvencattr.cgi?-chn=12&-brmode=4
 */
const char* HTTP_PREFFIX = "http://";
const char* FIXED_QUALITY = "/cgi-bin/hi3510/setvencattr.cgi?-chn=12&-brmode=4";

static int set_fixed_quality() {
    char buf[514] = {0};
    char answer[521];
    int rc, ret = 0;

    au_strcpy(buf, HTTP_PREFFIX, sizeof(buf));
    au_strcat(buf, ag_getCamIP(), sizeof(buf)-strlen(buf));
    au_strcat(buf, "/", sizeof(buf)-strlen(buf));
    au_strcat(buf, FIXED_QUALITY, sizeof(buf)-strlen(buf));

    t_ac_http_handler* h = ac_http_prepare_get_conn(buf, NULL);
    if(!h) return ret;

    int rpt = AC_HTTP_REPEATS;
    while(rpt) {
        rc = ac_perform_get_conn(h, answer, sizeof(answer));
        switch(rc) {
            case -1:        /* Retry */
                pu_log(LL_WARNING, "%s: Retry: attempt #%d", __FUNCTION__, rpt);
                rpt--;
                 sleep(1);
                break;
            case 0:         /* Error */
                rpt = 0;
                break;
            case 1:        /* Success */
                ret = 1;
                rpt = 0;
                break;
            default:
                pu_log(LL_ERROR, "%s: Unsupported RC = %d from ac_perform_get_conn()", __FUNCTION__, rc);
                rpt = 0;
                break;
        }
    }
    pu_log(LL_DEBUG, "%s: cam's request = %s cam's answer = %s", __FUNCTION__, buf, answer);
    ac_http_close_conn(h);
    return ret;
}

int ac_cam_restart() {
    return 0;
}

int ac_cam_init() {
    return set_fixed_quality();
}