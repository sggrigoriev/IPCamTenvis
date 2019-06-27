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
 * Created by gsg on 17/11/18.
 * Made to separate the 3rd-party lib and its memory leaks from own problems
 * Later on Send Files thread was added to the process
*/


#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <memory.h>
#include <stdio.h>
#include <ag_config/ag_settings.h>

#include "pu_logger.h"

#include "at_cam_alerts_reader.h"
#define PROC_NAME   "IPCAM_MONITOR"
/*
 * Debugging utility
 */
#define SHT_STCK_LEN 10

static uint32_t stck[SHT_STCK_LEN] = {0};
static int idx_inc(int i) {
    i = i+1;
    return (i < SHT_STCK_LEN)?i:0;
}
static int idx_dec(int i) {
    i = i -1;
    return (i < 0)?SHT_STCK_LEN-1:i;
}
static int idx=0;
void sht_add(uint32_t ctx) {
    stck[idx] = ctx;
    idx = idx_inc(idx);
}
static void printStack() {
    pu_log(LL_ERROR, "MONITOR.%s: Stack output:", __FUNCTION__);
    int i = idx;
    int stop = i;
    do {
        i = idx_dec(i);
        pu_log(LL_ERROR, "MONITOR.%s ctx[%d] = %d", __FUNCTION__, i, stck[i]);
    } while (i != stop);
}
/**/
void signalHandler( int signum ) {
    printf("\n%s.%s: Interrupt signal (%d) received. thread_id=%lu\n", PROC_NAME, __FUNCTION__, signum, pthread_self());
    printStack();
    exit(signum);
}
void totalStopp(int signum) {
    pu_log(LL_WARNING, "%s: EM cancelled by Agent", __FUNCTION__);
    at_mon_stop();
    exit(0);
}

static char* get_string(const char* buf) {
    IP_CTX_(100);
    if(!buf) return NULL;
    char* ret = strdup(buf);
    if(!ret) {
        pu_log(LL_ERROR, "%s: Not enough memory", __FUNCTION__);
    }
    IP_CTX_(101);
    return ret;
}
static int get_number(const char* buf) {
    IP_CTX_(102);
    int ret, amt;
    if(!buf) return -1;
    if(amt = sscanf(buf, "%d", &ret), amt!= 1) {
        pu_log(LL_ERROR, "%s: Wrong format of %s. Decimal number expected", __FUNCTION__, buf);
        return -1;
    }
    IP_CTX_(103);
    return ret;
}

static int get_input_params(int argc, char* argv[], input_params_t* p) {
    IP_CTX_(104);
    int ret = 0;
    mon_params_t i;

    if(argc != MON_SIZE) {
        pu_log(LL_ERROR, "%s: Wrong input parameters number %d. Expected %d", __FUNCTION__, argc, MON_SIZE);
        exit(-1);
    }

    for(i=MON_NAME; i < argc; i++) {
        switch(i) {
            case MON_NAME:
                if(p->process_name = get_string(argv[i]), !p->process_name) {
                    pu_log(LL_ERROR, "%s: Error in process name parameter. Exit", PROC_NAME);
                    goto on_exit;
                }
                break;
            case MON_AGENT_IP:
                if(p->agent_ip = get_string(argv[i]), !p->agent_ip) {
                    pu_log(LL_ERROR, "%s: Error in Agent IP parameter. Exit", PROC_NAME);
                    goto on_exit;
                }
                break;
            case MON_AGENT_PORT:
                if(p->agent_port = get_number(argv[i]), p->agent_port < 0) {
                    pu_log(LL_ERROR, "%s: Error in Agent port parameter. Exit", PROC_NAME);
                    goto on_exit;
                }
                break;
            case MON_CAM_IP:
                if(p->cam_ip = get_string(argv[i]), !p->cam_ip) {
                    pu_log(LL_ERROR, "%s: Error in Cam IP parameter. Exit", PROC_NAME);
                    goto on_exit;
                }
                break;
            case MON_CAM_PORT:
                if(p->cam_port = get_number(argv[i]), p->cam_port < 0) {
                    pu_log(LL_ERROR, "%s: Error in Cam port parameter. Exit", PROC_NAME);
                    goto on_exit;
                }
                    break;
            case MON_CAM_LOGIN:
                if(p->cam_login = get_string(argv[i]), !p->cam_login) {
                    pu_log(LL_ERROR, "%s: Error in Cam login parameter. Exit", PROC_NAME);
                    goto on_exit;
                }
                break;
            case MON_CAM_PASSWORD:
                if(p->cam_password = get_string(argv[i]), !p->cam_password) {
                    pu_log(LL_ERROR, "%s: Error in Cam password parameter. Exit", PROC_NAME);
                    goto on_exit;
                }
                break;
            case MON_CONTACT_URL:
                ag_saveMainURL(argv[i]);
                break;
            case MON_PROXY_ID:
                ag_saveProxyID(argv[i]);
                break;
            case MON_AUTH_TOKEN:
                ag_saveProxyAuthToken(argv[i]);
                break;
            default:
                pu_log(LL_ERROR, "%s: Wrong input parameters number %d. Expected %d", __FUNCTION__, argc, MON_SIZE);
                exit(-1);
        }
    }
    pu_log(LL_INFO, "Input parameters are: "
                    "\nname\t\t%s\nagent_ip\t\t%s\nagent_port\t\t%d\ncam_ip\t\t%s\ncam_port\t\t%d\ncam_login\t\t%s\ncam_password\t\t%s"
                    "\ncontact_url\t\t%s\nproxy_id\t\t%s\nauth_token\t\t%s",
                     p->process_name, p->agent_ip, p->agent_port, p->cam_ip, p->cam_port, p->cam_login, p->cam_password, 
                     ag_getMainURL(), ag_getProxyID(), ag_getProxyAuthToken()
            );
    ret = 1;
on_exit:
    IP_CTX_(105);
    return ret;
}

int main(int argc, char* argv[]) {
    signal(SIGSEGV, signalHandler);
    signal(SIGBUS, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGKILL, signalHandler);
    signal(SIGTERM, totalStopp);

    pu_start_logger("../log/monitor.log", 50000, LL_DEBUG);

    if(!ag_load_config(DEFAULT_CFG_FILE_NAME)) exit(-1);

    input_params_t pars;
    if (get_input_params(argc, argv, &pars)) at_mon_function(&pars); /* Endless loop inside the function */

    exit(0);
}