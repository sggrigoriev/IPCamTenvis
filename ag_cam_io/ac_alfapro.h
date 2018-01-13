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
 Created by gsg on 10/01/18.
*/

#ifndef IPCAMTENVIS_AC_ALFAPRO_H
#define IPCAMTENVIS_AC_ALFAPRO_H

#include <stdlib.h>

#include "ac_cam_types.h"

#include "ao_cma_cam.h"


t_at_rtsp_session* ac_alfaProInit();
void ac_alfaProDown(t_at_rtsp_session* sess);

int ac_alfaProOpenSession(t_at_rtsp_session* sess);
void ac_alfaProCloseSession(t_at_rtsp_session* sess);

int ac_alfaProOptions(t_at_rtsp_session* sess);
int ac_alfaProDescribe(t_at_rtsp_session* sess, char* descr, size_t size);
int ac_alfaProSetup(t_at_rtsp_session* sess, int client_port);
int ac_alfaProPlay(t_at_rtsp_session* sess);
int ac_alfaProTeardown(t_at_rtsp_session* sess);

/* [login:password@]ip:port/resolution/ */
const char* ac_makeAlfaProURL(char *url, size_t size, const char* ip, int port, const char* login, const char* pwd, t_ao_cam_res resolution);

#endif /* IPCAMTENVIS_AC_ALFAPRO_H */