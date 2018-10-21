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
 Created by gsg on 18/10/18.
*/



#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "lib_timer.h"
#include "cJSON.h"
#include "pu_logger.h"

#include "ag_defaults.h"
#include "aq_queues.h"
#include "ac_cam.h"

#include "at_cam_files_sender.h"

#define PT_THREAD_NAME "FILES_SENDER"

static volatile int is_stop = 0;
static pthread_t id;
static pthread_attr_t attr;

static pu_queue_t* from_main;
static pu_queue_msg_t q_msg[1024];    /* Buffer for messages received */

/*****************************************************************/
typedef struct {
    cJSON* root;
    cJSON* arr;
    char ft;
    int idx;
    int total;
} fld_t;

typedef struct {
    const char* name;
    const char* ext;
    char type;
    size_t size;
} fd_t;

typedef struct {
    cJSON* report;
    cJSON* array;
} rep_t;

static void clean_snap_n_video() {
    char path[256]={0};
    pu_log(LL_INFO, "%s: Clean SNAPSHOTS", PT_THREAD_NAME);
    snprintf(path, sizeof(path)-1, "%s/%s", DEFAULT_DT_FILES_PATH, DEFAULT_SNAP_DIR);
    ac_cam_clean_dir(path);

    pu_log(LL_INFO, "%s: Clean VIDEOS", PT_THREAD_NAME);
    snprintf(path, sizeof(path)-1, "%s/%s", DEFAULT_DT_FILES_PATH, DEFAULT_VIDEO_DIR);
    ac_cam_clean_dir(path);
}

static fld_t* open_flist(const char* msg) {
    fld_t* ret = NULL;

    cJSON* obj = cJSON_Parse(msg);
    if(!obj) return NULL;
    cJSON* type = cJSON_GetObjectItem(obj, "type");
    if(!type || (type->type != cJSON_String)) goto err;
    cJSON* arr = cJSON_GetObjectItem(obj, "filesList");
    if(!arr || (arr->type != cJSON_Array)) goto err;

    ret = calloc(sizeof(fld_t), 1);
    if(!ret) {
        pu_log(LL_ERROR, "%s: not enough memory", __FUNCTION__);
        goto err;
    }
    ret->root = obj;
    ret->arr = arr;
    ret->idx = 0;
    ret->total = cJSON_GetArraySize(arr);
    ret->ft = type->valuestring[0];
    return ret;
err:
    cJSON_Delete(obj);
    return ret;
}
static int get_next_f(fld_t* fld, fd_t* fd) {
    if(fld->idx < fld->total) {
        fd->name = cJSON_GetArrayItem(fld->arr, fld->idx)->valuestring;
        fd->type = fld->ft;
        fd->ext = ac_cam_get_file_ext(fd->name);
        fd->size = ac_get_file_size(fd->name);
        fld->idx++;
        return 1;
    }
    return 0;
}
static void close_fld(fld_t* fld) {
    cJSON_Delete(fld->root);
/* The rest fields are pointerd to the root */
    free(fld);
}

static int send_file(fd_t fd) {
/* Sending file to cloud */

/* Delete file if sent */
    if(!unlink(fd.name)) {
        pu_log(LL_DEBUG, "%s: File %s deleted", __FUNCTION__, fd.name);
    }
    else {
        pu_log(LL_ERROR, "%s: error deletion %s: %d - %s", __FUNCTION__, fd.name, errno, strerror(errno));
    }

    return 1;
}

static void* thread_function(void* params) {
    from_main = aq_get_gueue(AQ_ToSF);
    pu_queue_event_t events = pu_add_queue_event(pu_create_event_set(), AQ_ToSF);

    lib_timer_clock_t dir_clean_clock = {0};   /* timer for md/sd directories cleanup */
    lib_timer_init(&dir_clean_clock, DEFAULT_TO_FOR_DIR_CLEANUP);


    size_t len = sizeof(q_msg);
    while(!is_stop) {
        pu_queue_event_t ev;

        switch (ev = pu_wait_for_queues(events, 1)) {
            case AQ_ToSF: {
                while (pu_queue_pop(from_main, q_msg, &len)) {
                    pu_log(LL_INFO, "%s: received from Agent: %s ", PT_THREAD_NAME, q_msg);
                    fld_t* fld = open_flist(q_msg);
                    if(fld) {
                        fd_t fd;
                        while(get_next_f(fld, &fd)) send_file(fd);
                        close_fld(fld);
                    }
                    else {
                        pu_log(LL_ERROR, "%s: Can't get files list from %s ", PT_THREAD_NAME, q_msg);
                    }
                    len = sizeof(q_msg);
                 }
            }
                break;
            case AQ_Timeout:
                break;
            case AQ_STOP:
                is_stop = 1;
                pu_log(LL_INFO, "%s received STOP event. Terminated", PT_THREAD_NAME);
                break;
            default:
                pu_log(LL_ERROR, "%s: Undefined event %d on wait (to server)!", PT_THREAD_NAME, ev);
                break;
        }
/*3. MD/SD directories cleanup */
        if(lib_timer_alarm(dir_clean_clock)) {
            ac_delete_old_dirs();
            clean_snap_n_video();
            lib_timer_init(&dir_clean_clock, DEFAULT_TO_FOR_DIR_CLEANUP);
        }

    }
    return NULL;
}

int at_start_sf() {
    if(pthread_attr_init(&attr)) return 0;
    if(pthread_create(&id, &attr, &thread_function, NULL)) return 0;
    return 1;

}
void at_stop_sf() {
    void *ret;

    at_set_stop_sf();
    pthread_join(id, &ret);
    pthread_attr_destroy(&attr);
}
void at_set_stop_sf() {
    is_stop = 1;
}



