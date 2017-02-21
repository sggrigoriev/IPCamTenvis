//
// Created by gsg on 06/12/16.
//
#include <pthread.h>
#include <string.h>

#include "pu_logger.h"
#include "pt_queues.h"
#include "pc_defaults.h"
#include "pc_settings.h"
#include "pf_traffic_proc.h"
#include "ph_manager.h"
#include "pt_server_write.h"

#define PT_THREAD_NAME "SERVER_WRITE"

////////////////////////////
static pthread_t id;
static pthread_attr_t attr;
static volatile int stop;
static pu_queue_msg_t msg[PROXY_MAX_MSG_LEN];

static pu_queue_t* to_main;       //to write answers from cloud
static pu_queue_t* from_main;     //to read and pass
static pu_queue_t* to_agent;

static void* write_proc(void* params);
static void conn_state_notf_to_agent(int connect, const char* device_id);  //sends to the cloud notification about the connection state

int start_server_write() {

    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &write_proc, NULL)) return 0;
    return 1;
}

void stop_server_write() {
    void *ret;

    set_stop_server_write();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}

void set_stop_server_write() {
    stop = 1;
}

static void* write_proc(void* params) {
    pu_queue_event_t events;

    stop = 0;
    from_main = pt_get_gueue(PS_ToServerQueue);
    to_main = pt_get_gueue(PS_FromServerQueue);
    to_agent = pt_get_gueue(PS_ToAgentQueue);

    events = pu_add_queue_event(pu_create_event_set(), PS_ToServerQueue);

    char devid[LIB_HTTP_DEVICE_ID_SIZE];
    char fwver[DEFAULT_FW_VERSION_SIZE];
    pc_getProxyDeviceID(devid, sizeof(devid));
    pc_getFWVersion(fwver, sizeof(fwver));

//Main write loop
    conn_state_notf_to_agent(1, devid);
    while(!stop) {
        pu_queue_event_t ev;
        switch (ev = pu_wait_for_queues(events, DEFAULT_SERVER_WRITE_THREAD_TO_SEC)) {
            case PS_ToServerQueue: {
                size_t len = sizeof(msg);
                while (pu_queue_pop(from_main, msg, &len)) {
                    pu_log(LL_DEBUG, "%s: Got from from main by server_write_thread: %s", PT_THREAD_NAME, msg);
//Sending with retries loop
                    int out = 0;
    //Adding the head to message
                    pf_add_proxy_head(msg, sizeof(msg), devid, 11038);

                    while(!stop && !out) {
                        char resp[PROXY_MAX_MSG_LEN];
                        if(!ph_write(msg, resp, sizeof(resp))) {    //no connection: reconnect forever
                            pu_log(LL_ERROR, "%s: Error sending. Reconnect", PT_THREAD_NAME);
                            conn_state_notf_to_agent(0, devid);
                            ph_reconnect();    //loop until the succ inside
                            conn_state_notf_to_agent(1, devid);
                            out = 0;
                        }
                        else {  //data has been written
                            pu_log(LL_INFO, "%s: Sent to cloud: %s", PT_THREAD_NAME, msg);
                            if (strlen(resp) > 0) {
                                pu_log(LL_INFO, "%s: Answer from cloud forwarded to Agent: %s", PT_THREAD_NAME, resp);
                                pu_queue_push(to_agent, resp, strlen(resp)+1);
                                out = 1;
                            }
                            len = sizeof(msg);
                        }
                    }
                }
                break;
            }
            case PS_Timeout:
                pu_log(LL_WARNING, "%s: timeout on waiting to server queue", PT_THREAD_NAME);
                break;
            case PS_STOP:
                stop = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", PT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait (to server)!", PT_THREAD_NAME, ev);
                break;
        }
    }
    pthread_exit(NULL);
}
static char* conn_msg_1 = "{\"gw_cloudConnection\":[{\"deviceId\":\"";
static char* conn_msg_2 = "\",\"paramsMap\":{\"cloudConnection\":\"";
static char* conn_yes = "connected";
static char* conn_no = "disconnected";
static char* conn_msg_3 = "\"}}]}";
static void conn_state_notf_to_agent(int connect, const char* device_id) {  //sends to the cloud notification about the connection state
    char msg[LIB_HTTP_MAX_MSG_SIZE];
    strncpy(msg, conn_msg_1, sizeof(msg)-1);
    strncat(msg, device_id, sizeof(msg)-strlen(msg)-1);
    strncat(msg, conn_msg_2, sizeof(msg)-strlen(msg)-1);
    if(connect)
        strncat(msg, conn_yes, sizeof(msg)-strlen(msg)-1);
    else
        strncat(msg, conn_no, sizeof(msg)-strlen(msg)-1);
    strncat(msg, conn_msg_3, sizeof(msg)-strlen(msg)-1);

    pu_queue_push(to_agent, msg, strlen(msg)+1);
}