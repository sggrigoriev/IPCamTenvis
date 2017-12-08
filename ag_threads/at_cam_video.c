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

static pthread_t id = 0;
static pthread_attr_t attr;

static volatile int stop = 1;       /* Thread stop flag */

typedef enum {
    AT_ERROR,
    AT_INITIAL,                 /* No connection info */
    AT_GOT_PROXY_INFO,              /* Got proxy/auth info - could start params */
    AT_GOT_VIDEO_CONN_INFO,         /* Got parameters! */
    AT_READY,
    AT_PLAY
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
static t_mgr_state start_ws_thread() {
    if(!start_ws(vs_host, vs_port, "/streaming/camera", vs_session_id)) {
        pu_log(LL_ERROR, "%s - %s: Error start WEB socket connector, exit.", AT_THREAD_NAME, __FUNCTION__);
        return AT_ERROR;
    }
    return AT_READY;
}
static t_mgr_state start_vc_therad() {
    if(own_status !AT_READY)
    if (!at_start_video_connector(vs_host, vs_port, vs_session_id)) {
        pu_log(LL_ERROR, "%s - %s: Error start video connector, exit.", AT_THREAD_NAME, __FUNCTION__);
        return AT_ERROR;
    }
    return AT_PLAY;
}
/* Get video params params from cloud: 3 steps from https://presence.atlassian.net/wiki/spaces/EM/pages/164823041/Setup+IP+Camera+connection */
static t_mgr_state get_vs_conn_params(const char* cloud_conn_sring, char* video_host, size_t vh_size, int* video_port, char* video_session, size_t vs_size) {
    /* should be AT_GOT_VIDEO_CONN_INFO */
    return AT_ERROR;
}
static t_mgr_state process_agent_message(const char* msg) {
    t_ao_msg data;
    if(own_status != AT_INITIAL) {
        pu_log(LL_ERROR, "%s - %s: Bad status = %d! Sould be %d", AT_THREAD_NAME, __FUNCTION__, own_status, AT_INITIAL);
        return AT_ERROR;
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
    if(strlen(ag_getProxyID()) && strlen(ag_getProxyAuthToken())) return AT_GOT_PROXY_INFO;
    return own_status;
}
static t_mgr_state process_ws_message(const char* msg) {
    if(own_status != AT_READY) {
        pu_log(LL_ERROR, "%s - %s: Bad status = %d! Sould be %d", AT_THREAD_NAME, __FUNCTION__, own_status, AT_READY);
        return AT_ERROR;
    }
    if(!strcmp(msg, DEFAULT_WC_START_PLAY)) return start_vc_therad();

    if(!strcmp(msg, DEFAULT_WC_STOP_PLAY)) {
        at_stop_video_connector();
        pu_log(LL_INFO, "%s: Video connector stop", AT_THREAD_NAME);
        return AT_READY;
    }
    return own_status;
}

static void* main_thread(void* params) {

    unsigned int events_timeout = 1; /* Wait 1 second - timeouts for respond from cloud should be enabled! */
    pu_queue_msg_t msg[LIB_HTTP_MAX_MSG_SIZE] = {0};    /* The only main thread's buffer! */

    pu_queue_event_t events;
    events = pu_add_queue_event(pu_create_event_set(), AQ_FromWS);
    events = pu_add_queue_event(events, AQ_ToVideoMgr);

    from_agent = aq_get_gueue(AQ_ToVideoMgr);
    from_ws = aq_get_gueue(AQ_FromWS);

    own_status = AT_INITIAL;

    while(!stop) {
        size_t len = sizeof(msg);    /* (re)set max message lenght */
        pu_queue_event_t ev;

        switch (ev=pu_wait_for_queues(events, events_timeout)) {
            case AQ_ToVideoMgr:     /* Messages from Agent Main */
                while(pu_queue_pop(from_agent, msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Agent main %s", AT_THREAD_NAME, msg);
                    own_status = process_agent_message(msg);
                    len = sizeof(msg);
                }
                break;
            case AQ_FromWS:
                while(pu_queue_pop(from_ws, msg, &len)) {
                    pu_log(LL_DEBUG, "%s: got message from the Web socket thread %s", AT_THREAD_NAME, msg);
                    own_status = process_ws_message(msg);
                    len = sizeof(msg);
                }
                break;
            case AQ_Timeout:
                break;
            case AQ_STOP:
                stop = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", AT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait.", AT_THREAD_NAME, ev);
                break;
        }
        switch(own_status) {        /* State machine */
            case AT_GOT_PROXY_INFO:
                own_status = get_vs_conn_params(DEFAULT_CLOUD_CONN_STRING, vs_host, sizeof(vs_host), &vs_port, vs_session_id, sizeof(vs_session_id)))
                break;
            case AT_GOT_VIDEO_CONN_INFO:
                own_status = start_ws_thread(); /* -> READY */
                break;
            case AT_READY:
                if(!is_ws_run()) {
                    pu_log(LL_WARNING, "%s: Wideo socket tread restart", AT_THREAD_NAME);
                    own_status = start_ws_thread();
                }
                break;
            case AT_PLAY:
                if(!is_ws_run()) {
                    pu_log(LL_WARNING, "%s: Video connector thread restart", AT_THREAD_NAME);
                    own_status = start_ws_thread();
                }
                break;
            case AT_ERROR:
                stop = 1;
            default:
                break;
        }
        break;

    }
/* shutdown procedure */
    at_stop_video_connector();
    stop_ws();
    pthread_exit(NULL);
#endif
}

/**********************************************************************
 * Public functions
 */

int at_start_video_mgr() {
    if(is_video_mgr_run() ) {
        pu_log(LL_WARNING, "%s - %s The thread is already runninng", AT_THREAD_NAME, __FUNCTION__);
        return 1;
    }
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &main_thread, NULL)) return 0;

    stop = 0;
    return 1;
}
int at_stop_video_mgr() {
    void* ret;
    if(!is_video_mgr_run() ) {
        pu_log(LL_WARNING, "%s - %s: The thread is already stop. Stop ignored", AT_THREAD_NAME, __FUNCTION__);
        return 1;
    }
    stop = 1;
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);

    return 1;
}
void at_set_stop_video_mgr() {
    stop = 1;
}
int is_video_mgr_run() {
    return id > 0;
}
