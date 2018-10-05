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
 Created by gsg on 30/10/17.
*/
#include <string.h>
#include <ctype.h>

#include "pu_logger.h"

#include "au_string.h"
#include "ag_settings.h"

#include "ao_cma_cam.h"

/* Cam's commands names */
#define CMD_SD_NAME     "sounddet"
#define CMD_MD_NAME     "cfgalertmd"
#define CMD_SNAP_NAME   "snapshot?&strm=0&q=0"

#define PAR_READ    "?list"
/* Cam's parameters names */
#define PAR_SD_ENABLE_NAME      "enable"
#define PAR_SENSITIVITY_NAME    "sensitivity"
#define TAPE_CH_NAME            "tapech"
#define REC_CH_NAME             "recch"
#define DEAL_MODE_NAME          "dealmode"
#define TS0_NAME                "ts0"
#define CH_NAME                 "chn"
#define RECT0_NAME              "rect0"

#define PAR_MDSD_ON   "ts0 + 0; 00:00:00-23:59:59"
#define PAR_MD_RECT0 "rect0 0, 0, 999, 999, 5"
#define PAR_SD_SENSITIVITY PAR_SENSITIVITY_NAME "5"


/*
 * Notify Agent about the event
 * if start_date or end_date is 0 fields are ignored
 * {"alertName" : "AC_CAM_STOP_MD", "startDate" : 1537627300, "endDate" : 1537627488}
 */
const char* ao_make_cam_alert(t_ac_cam_events event, time_t start_date, time_t end_date, char* buf, size_t size) {
    const char* alert = "{\"alertName\" : \"%s\"}";
    const char* alert_start = "{\"alertName\" : \"%s\", \"startDate\" : %lu}";
    const char* alert_stop = "{\"alertName\" : \"%s\", \"startDate\" : %lu, \"endDate\" : %lu}";

    if(!start_date)
        snprintf(buf, size-1, alert, ac_cam_evens2string(event));
    else if(!end_date)
        snprintf(buf, size-1, alert_start, ac_cam_evens2string(event), start_date);
    else
        snprintf(buf, size-1, alert_stop, ac_cam_evens2string(event), start_date, end_date);

    buf[size] = '\0';
    return buf;
}

/************************************************************
   Camera commands
*/
typedef struct {
    int first, first_after;
} pos_t;
/* first -> name[0] last ->\n- */
static pos_t find_par(const char* par_name, const char* lst) {
    pos_t ret = {-1,-1};
    if(ret.first = au_findSubstr(lst, par_name, AU_CASE), ret.first < 0) return ret;
    if(ret.first_after = au_findSubstr(lst+ret.first, "\n", AU_CASE), ret.first_after < 0) {
        pu_log(LL_ERROR, "%s: Cam format error: %s is not rerminated by CRLF in %s!", __FUNCTION__, par_name, lst);
        ret.first = -1;
    }
    return ret;
}
/* find the lase number parameter */
static pos_t find_in_rect(int par_idx, pos_t pos, const char* lst) {
    pos_t ret = {-1,-1};
    if(par_idx != AO_CAM_PAR_MD_SENS) return ret;
    ret.first_after = pos.first_after;
    for(ret.first = ret.first_after-1; (ret.first > pos.first) && isdigit(lst[ret.first]); ret.first--);
    ret.first++;
    return ret;
}

/* to take first ptr == lst. Return NULL if no more params or pointer to the next one */
static char* get_next_param(char* buf, size_t size, char* ptr) {
    pos_t pos={0,0};
    buf[0] = '\0';
    while(!isalpha(ptr[pos.first]) && (ptr[pos.first] != '\n') && (ptr[pos.first] != '\0')) pos.first++;
    if(!isalpha(ptr[pos.first])) return NULL;
    pos.first_after = pos.first;
    while(isalpha(ptr[pos.first_after++]));
    memcpy(buf, ptr+pos.first, (size_t)(pos.first_after-pos.first));
    buf[pos.first_after] = '\0';
    return (strlen(buf))?ptr+(pos.first_after-pos.first):NULL;
}
/* get int from list in pos coordinates*/
static int get_int_value(pos_t pos, const char* lst) {
    int ret;
    if(sscanf(lst+pos.first, "%d", &ret) != 1) return 0;
    return ret;
}
/* find start-stop of number in lst[pos] and replace it to val */
static char* replace_value(pos_t pos, int val, char* lst) {
    char* ret = NULL;
    char buf[256]={0};
    while(!isdigit(lst[pos.first]) && lst[pos.first] != '\0') pos.first++;
    if(!isdigit(lst[pos.first])) return NULL;
    strncpy(buf, lst, (size_t)(pos.first));
    sprintf(buf+pos.first, "%d%s", val, lst+pos.first_after);
    if(ret = calloc(strlen(buf)+1, 1), !ret) {
        pu_log(LL_ERROR, "%s: Not enough memory", __FUNCTION__);
        return NULL;
    }
    free(lst);
    return ret;
}
/* Just append string par_val to lst */
static char* append_param(const char* par_val, char* lst) {
    char* ret = calloc(strlen(lst)+strlen(par_val)+2, 1); /* +1 for params delimeters */
    if(!ret) {
        pu_log(LL_ERROR, "%s: Not enough memory.", __FUNCTION__);
        return NULL;
    }
    strcpy(ret, lst);
    strcat(ret-1, par_val); /* end list got \n\n at the end. Skip the first \n */
    strcat(ret, "\n");
    free(lst);
    return ret;
}
/* remove smth in pos boundary */
static char* remove_param(pos_t pos, char* lst) {
    char* ret = calloc(strlen(lst)+1, 1);
    if(!ret) {
        pu_log(LL_ERROR, "%s: Not enogh memory.", __FUNCTION__);
        return NULL;
    }
    strncpy(ret, lst, (size_t)(pos.first));
    ret[pos.first] = '\0';
    strcat(ret+pos.first, lst+pos.first_after+1);  /* Just skip \n after parameter */
    free(lst);
    return ret;
}

char* ao_make_cam_uri(int cmd_id, int read_pars) {
    char buf[128]={0};
    char* name;
    char* ret=NULL;
    switch(cmd_id) {
        case AO_CAM_CMD_SNAPSHOT:
            name = CMD_SNAP_NAME;
            break;
        case AO_CAM_CMD_MD:
            name = (read_pars)?CMD_MD_NAME PAR_READ:CMD_MD_NAME;
            break;
        case AO_CAM_CMD_SD:
            name = (read_pars)?CMD_SD_NAME PAR_READ:CMD_MD_NAME;
            break;
        default:
            pu_log(LL_ERROR, "%s: Unrecognized cmd_id = %d", __FUNCTION__, cmd_id);
            return NULL;
    }
    snprintf(buf, sizeof(buf)-1, "http://%s:%d/%s", ag_getCamIP(), ag_getCamPort(), name);
    buf[sizeof(buf)-1] = '\0';

    if(ret = strdup(buf), !ret) {
        pu_log(LL_ERROR, "%s: Not enough memory.", __FUNCTION__);
        return NULL;
    }
    return ret;
}
/* [-]name value\n...\n\n list */
char* ao_update_params_list(int cmd_id, int par_id, int par_value, char* lst) {
    pos_t pos;

    switch (par_id) {
        case AO_CAM_PAR_MD_SENS:
            pos = find_par(RECT0_NAME, lst);
            if(pos.first < 0) {
                lst = append_param(PAR_MD_RECT0, lst);
                pos = find_par(RECT0_NAME, lst);
            }
            pos = find_in_rect(AO_CAM_PAR_MD_SENS, pos, lst);
            if(pos.first < 0) {
                pu_log(LL_ERROR, "%s: Param_id %d not found for command_id %d in %s", __FUNCTION__, AO_CAM_PAR_MD_SENS, cmd_id, lst);
                return NULL;
            }
            return replace_value(pos, par_value, lst);
        case AO_CAM_PAR_MD_ON:
        case AO_CAM_PAR_SD_ON:
            pos = find_par(TS0_NAME, lst);
            if(pos.first > 0) {
                lst = remove_param(pos, lst);
            }
            return append_param(PAR_MDSD_ON, lst);
        case AO_CAM_PAR_MD_OFF:
        case AO_CAM_PAR_SD_OFF:
            pos = find_par(TS0_NAME, lst);
            if(pos.first > 0) {
                lst = remove_param(pos, lst);
            }
            return lst;
        case AO_CAM_PAR_SD_SENS:
            pos = find_par(PAR_SENSITIVITY_NAME, lst);
            if(pos.first < 0) {
                lst = append_param(PAR_SD_SENSITIVITY, lst);
                pos = find_par(PAR_SENSITIVITY_NAME, lst);
            }
            return replace_value(pos, par_value, lst);
        default:
            pu_log(LL_ERROR, "%s: Unrecognizeable parameter id %d for command %d", __FUNCTION__, cmd_id, par_id);
            return NULL;
    };
    return NULL; /* On some case... */
}
/*
 * Transform name value\n...\n\n to
 * name=value&...
 */
char* ao_make_params_from_list(int cmd_id, char* lst) {
    char* ret = calloc(strlen(lst), 1);
    if(!ret) {
        pu_log(LL_ERROR, "%s: Not enough memory.", __FUNCTION__);
        return NULL;
    }
    char* ptr = lst;
    char buf[128];
    while(ptr=get_next_param(buf, sizeof(buf)-1, ptr), !ptr) {
        if(ptr != lst) strcat(ret, "&");
        int space = au_findSubstr(buf, " ", AU_CASE);
        if(space < 0) {
            pu_log(LL_ERROR, "%s:no space in parameter %s", __FUNCTION__, buf);
            return NULL;
        }
        buf[space] = '=';
        strcat(ret, buf);
    }
    free(lst);
    return ret;
}
int ao_get_param_value_from_list(int cmd_id, int par_id, const char* lst) {
    pos_t pos;
    switch(par_id) {
        case AO_CAM_PAR_MD_SENS:
            pos = find_par(RECT0_NAME, lst);
            if(pos.first < 0) {
                return 0;
            }
            pos = find_in_rect(AO_CAM_PAR_MD_SENS, pos, lst);
            if(pos.first < 0) {
                return 0;
            }
            return get_int_value(pos, lst);
        case AO_CAM_PAR_MD_ON:
        case AO_CAM_PAR_SD_ON:
        case AO_CAM_PAR_MD_OFF:
        case AO_CAM_PAR_SD_OFF:
            pos = find_par(TS0_NAME, lst);
            return pos.first > 0;
        case AO_CAM_PAR_SD_SENS:
            pos = find_par(PAR_SENSITIVITY_NAME, lst);
            if(pos.first < 0) return 0;
            return get_int_value(pos, lst);
        default:
            pu_log(LL_ERROR, "%s: Unrecognizeable parameter id %d for command %d", __FUNCTION__, cmd_id, par_id);
            return 0;
    }
}