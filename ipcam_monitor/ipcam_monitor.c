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
 * Made to separate the 3rd-party lib and its memory leaks from own problems
 * Created by gsg on 17/11/18.
*/


#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <memory.h>
#include <stdio.h>

#include "pu_logger.h"

#include "at_cam_alerts_reader.h"

/*
 * Debugging utility
 */
#define PROC_NAME   "IPCAM_MONITOR"
volatile uint32_t contextId = 0;
void signalHandler( int signum ) {
    pu_log(LL_ERROR, "MONITOR.%s: Interrupt signal (%d) received. ContextId=%d thread_id=%lu\n", __FUNCTION__, signum, contextId, pthread_self());
    exit(signum);
}
void totalStopp(int signum) {
    pu_log(LL_WARNING, "%s: EM cancelled by Agent", __FUNCTION__);
    at_mon_stop();
    exit(0);
}

static char* get_string(const char* buf) {
    if(!buf) return NULL;
    char* ret = strdup(buf);
    if(!ret) {
        pu_log(LL_ERROR, "%s: Not enough memory", __FUNCTION__);
    }
    return ret;
}
static int get_number(const char* buf) {
    int ret, amt;
    if(!buf) return -1;
    if(amt = sscanf(buf, "%d", &ret), amt!= 1) {
        pu_log(LL_ERROR, "%s: Wrong format of %s. Decimal number expected", __FUNCTION__, buf);
        return -1;
    }
    return ret;
}

static int get_input_params(int argc, char* argv[], input_params_t* p) {
    int ret = 0;
    mon_params_t i;

    if(argc != MON_SIZE) {
        pu_log(LL_ERROR, "%s: Wrong input parameters number %d. Expected %d: name agent_ip agent_port cam_ip, cam_login cam_password", __FUNCTION__, argc, MON_SIZE);
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
            default:
                pu_log(LL_ERROR, "%s: Wrong input parameters number %d. Expected %d: name agent_ip agent_port cam_ip cam_login cam_password", __FUNCTION__, argc, MON_SIZE);
                exit(-1);
        }
    }
    pu_log(LL_INFO, "Input parameters are: \nname\t\t%s\nagent_ip\t\t%s\nagent_port\t\t%d\ncam_ip\t\t%s\ncam_port\t\t%d\ncam_login\t\t%s\ncam_password\t\t%s",
           p->process_name, p->agent_ip, p->agent_port, p->cam_ip, p->cam_port, p->cam_login, p->cam_password);
    ret = 1;
on_exit:
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



    input_params_t pars;
    if (get_input_params(argc, argv, &pars)) at_mon_function(&pars); /* Endless loop inside the function */

    exit(0);
}