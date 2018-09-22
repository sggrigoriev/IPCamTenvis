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
 Created by gsg on 21/09/18.
*/

#include <signal.h>
#include <sys/types.h>

#include <DvsConn.h>

#include "event_monitor_module.h"

/*************************************************/
CDvsConnManager g_DevMgr;
DVSCONN *pConn = NULL;
int g_pipeFd[2];

int em_init(const char* ip) {
    signal(SIGPIPE, SIG_IGN);
    pipe(g_pipeFd);
    SetEventListener(g_pipeFd[1]);

    g_DevMgr.Initialize();

    pConn = g_DevMgr.DeviceAdd();
    strcpy(pConn->cUser, "admin");
    strcpy(pConn->cPassword, "admin");
    strcpy(pConn->cHost, ip);

    return ((pConn && pConn->Connect()) == 0);
}
int em_function(int to_sec) {
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(g_pipeFd[0], &rfds);
    tv.tv_sec = to_sec; tv.tv_usec = 0;
    int ret = select(g_pipeFd[0]+1, &rfds, NULL, NULL, &tv);
    if( ret > 0) {
        struct EventBuff {
            uint32 event;
            uint32 len;
            union {
                void *pObj;
                NOTIFYSTRUCT noti;
            };
        } eBuff;

        int ok = 0;

        /* Read contents of event from pipe */
        if(read(g_pipeFd[0], &eBuff, 8) == 8) {
            if(eBuff.len)
                ok = read(g_pipeFd[0], &eBuff.noti, eBuff.len) == eBuff.len;
            else
                ok = read(g_pipeFd[0], &eBuff.pObj, sizeof(void*)) == sizeof(void*);
        }

        if(!ok) {
            return EMM_READ_ERROR;
        }
        else
            switch(eBuff.event) {
                case DEVICEEVENT_PEERCLOSED:
                case DEVICEEVENT_NO_RESPONSE:
                        return EMM_NO_RESPOND;
                case NOTIFICATION_ALARM:
                    return (eBuff.noti.alarm.onoff)?-eBuff.noti.alarm.event : EMM_ALRM_IGNOR;
                default:
                    return eBuff.event;
            }
    }
    else if(ret < 0) return EMM_SELECT_ERR;
    return EMM_TIMEOUT; // ret == 0 - most popular case
}

void em_deinit() {
    g_DevMgr[0]->Disconnect();
    g_DevMgr.DeviceRemove(g_DevMgr[0]);
}
int em_connect() {
    return ((pConn && pConn->Connect()) == 0);
}
void em_disconnect() {

}

