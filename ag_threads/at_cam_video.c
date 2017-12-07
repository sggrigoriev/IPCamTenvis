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
 Created by gsg on 17/10/17.
*/

#include <pthread.h>
#include <memory.h>

#include "pu_logger.h"
#include "pu_queue.h"

#include "aq_queues.h"
#include "ag_settings.h"
#include "ao_cmd_data.h"
#include "ao_cmd_cloud.h"
#include "at_video_connector.h"

#include "at_cam_video.h"
#include "at_ws.h"

#define AT_THREAD_NAME "VIDEO_MANAGER"

/*************************************************************************
 * Local data
 */

static pthread_t id;
static pthread_attr_t attr;

static volatile int stop;       /* Thread stop flag */

typedef enum {
    AT_INITIAL,                 /* No connection info */
    AT_GOT_PROXY_INFO,              /* Got proxy/auth info - could start params */
    AT_GOT_VIDEO_CONN_INFO,         /* Got parameters! */
    AT_READY
} t_mgr_state;

static t_mgr_state own_status = AT_INITIAL;

static pu_queue_t* from_agent;
static pu_queue_t* from_ws;

static char vs_host[4000] = {0};
static int vs_port;
static char vs_session_id[100] = {0};

/*******************************************************************
* Local functions implementation
*/

/* Get video params params from cloud: 3 steps from https://presence.atlassian.net/wiki/spaces/EM/pages/164823041/Setup+IP+Camera+connection */
static int get_vs_conn_params(const char* cloud_conn_sring, char* video_host, size_t vh_size, int* video_port, char* video_session, size_t vs_size) {
    return 1;
}
static void process_agent_message(const char* msg) {
    t_ao_msg data;
    if(own_status != AT_INITIAL) {
        pu_log(LL_ERROR, "%s - %s: Bad status = %d! Sould be %d", AT_THREAD_NAME, __FUNCTION__, own_status, AT_INITIAL);
        return;
    }
    t_ao_msg_type rc = ao_agent_decode(msg, &data);
    switch (rc) {
        case AO_IN_PROXY_ID:
            ag_saveProxyID(data.in_proxy_id.proxy_device_id);
            break;
        case AO_IN_PROXY_AUTH:
            ag_saveProxyAuthToken(data.in_proxy_auth.proxy_auth);
            break;
        default:
            pu_log(LL_ERROR, "%s: Unrecognized message type %d from Agent. Ignored", AT_THREAD_NAME, rc);
            break;
    }
    if(strlen(ag_getProxyID()) && strlen(ag_getProxyAuthToken())) own_status = AT_GOT_PROXY_INFO;
}
static void process_ws_message(const char* msg) {
    if(own_status != AT_READY) {
        pu_log(LL_ERROR, "%s - %s: Bad status = %d! Sould be %d", AT_THREAD_NAME, __FUNCTION__, own_status, AT_READY);
        return;
    }
    if(!strcmp(msg, DEFAULT_WC_START_PLAY)) {
        if(!at_start_video_connector(vs_host, vs_port, vs_session_id)) {
            pu_log(LL_ERROR, "%s - %s: Error start video connector, exit.", AT_THREAD_NAME, __FUNCTION__);
            stop = 1;
        }
        return;
    }
    if(!strcmp(msg, DEFAULT_WC_STOP_PLAY)) {
        at_stop_video_connector();
        pu_log(LL_INFO, "%s: Video connector stop", AT_THREAD_NAME);
    }
    return;
}
#ifdef LOCAL_TEST
static void main_thread()
#else
static void* main_thread(void* params)
#endif
{
    stop = 0;

    unsigned int events_timeout = 1; /* Wait 1 second - timeouts for respond from cloud should be enabled! */
    pu_queue_event_t events;
    pu_queue_msg_t msg[LIB_HTTP_MAX_MSG_SIZE] = {0};    /* The only main thread's buffer! */
    from_agent = aq_get_gueue(AQ_ToVideoMgr);
    from_ws = aq_get_gueue(AQ_FromWS);
    events = pu_add_queue_event(pu_create_event_set(), AQ_FromWS);
#ifndef LOCAL_TEST
    events = pu_add_queue_event(events, AQ_ToVideoMgr);
#endif

#ifdef LOCAL_TEST
    own_status = AT_GOT_VIDEO_CONN_INFO;
#endif

    while(!stop) {
        size_t len = sizeof(msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;

        switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case AQ_ToVideoMgr:
                while(pu_queue_pop(from_agent, msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Agent main %s", AT_THREAD_NAME, msg);
                    process_agent_message(msg);
                    len = sizeof(msg);
                }
                if(own_status == AT_GOT_PROXY_INFO) {
                    if(!get_vs_conn_params(DEFAULT_CLOUD_CONN_STRING, vs_host, sizeof(vs_host), &vs_port, vs_session_id, sizeof(vs_session_id))) {
                        stop = 1;
                    }
                    own_status = AT_GOT_VIDEO_CONN_INFO;
                    if(!start_ws(vs_host, vs_port, "/streaming/camera", vs_session_id)) {
                        stop = 1;
                    }
                    own_status = AT_READY;
                }
                break;
            case AQ_FromWS:
                while(pu_queue_pop(from_ws, msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Web socket thread %s", AT_THREAD_NAME, msg);
                    process_ws_message(msg);
                    len = sizeof(msg);
                }
                break;
            case AQ_Timeout:
#ifdef LOCAL_TEST
                    if(!start_ws(vs_host, vs_port, "/streaming/camera", vs_session_id)) {
                        stop = 1;
                    }
                    own_status = AT_READY;
 #endif
                 break;
            case AQ_STOP:
                stop = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", AT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait.", AT_THREAD_NAME, ev);
                break;
        }
    }
/* shutdown procedure */
    at_stop_video_connector();
    stop_ws();
#ifdef LOCAL_TEST
    return;
#else
    pthread_exit(NULL);
#endif
}

/**********************************************************************
 * Public functions
 */

int at_start_video_mgr(const char* host, int port, const char* session_id, const char* proxy_id, const char* proxy_auth) {
    if(host) strncpy(vs_host, host, sizeof(vs_host)-1);
    if(port >=0) vs_port = port;
    if(session_id) strncpy(vs_session_id, session_id, sizeof(vs_session_id)-1);
    if(proxy_id) ag_saveProxyID(proxy_id);
    if(proxy_auth) ag_saveProxyAuthToken(proxy_auth);
#ifdef LOCAL_TEST
    main_thread();
#else
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &main_thread, NULL)) return 0;
#endif
    return 1;
}
int at_stop_video_mgr() {
    void* ret;

    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    return 1;
}
void at_set_stop_video_mgr() {
    stop = 1;
}

