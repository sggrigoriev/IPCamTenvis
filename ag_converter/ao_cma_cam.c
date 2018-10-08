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

#include "cJSON.h"
#include "pu_logger.h"

#include "au_string.h"
#include "ag_settings.h"

#include "ao_cma_cam.h"

/* Alert thread constants */
#define ALERT_NAME   "alertName"
#define ALERT_START  "startDate"
#define ALERT_END    "endDate"

/* Cam's commands names */
#define CMD_SD_NAME     "sounddet"
#define CMD_MD_NAME     "cfgalertmd"
#define CMD_SNAP_NAME   "snapshot?&strm=0&q=0"

#define PAR_READ    "?list=1"
/* Cam's parameters names */
#define PAR_SD_ENABLE_NAME      "enable"
#define PAR_SENSITIVITY_NAME    "sensitivity"
#define TAPE_CH_NAME            "tapech"
#define REC_CH_NAME             "recch"
#define DEAL_MODE_NAME          "dealmode"
#define TS0_NAME                "ts0"
#define TS1_NAME                "ts1"
#define TS2_NAME                "ts2"
#define TS3_NAME                "ts3"
#define CH_NAME                 "chn"
#define RECT0_NAME              "rect0"

#define PAR_MDSD_ON         "+ 0; 00:00:00-23:59:59"
#define PAR_MDSD_OFF        "ts0="
#define PAR_MD_RECT0        "rect0=0,0,999,999, 5"
#define PAR_SD_SENSITIVITY  PAR_SENSITIVITY_NAME "=5"

static const char* PAR_ARRAY[EP_SIZE] = {
    "???",
    "recch", "tapech",
    "ts0", "ts1", "ts2", "ts3",
    "dealmode", "enable", "sensitivity",
    "rect0", "rect1", "rect2", "rect3",
    "chn"
};

typedef struct {
    int x0, x1, x2, x3, sensitivity;
} rect_t;

typedef struct {
    int recch;
    int tapech;
    char* ts[4];
    int dealmode;
    rect_t* rect[4];    /* Null in rect[i] means 'rectI=' */
    int chn;
} md_par_t;
typedef struct {
    int tapech;
    int recch;
    int dealmode;
    char* ts[4];        /* NULL in ts[i] means 'tsI=' */
    int enable;
    int sensitivity;
} sd_par_t;

static md_par_t MD_PARAMS = {0};
static sd_par_t SD_PARAMS = {0};


/*
 * Notify Agent about the event
 * if start_date or end_date is 0 fields are ignored
 * {"alertName" : "AC_CAM_STOP_MD", "startDate" : 1537627300, "endDate" : 1537627488}
 */
const char* ao_make_cam_alert(t_ac_cam_events event, time_t start_date, time_t end_date, char* buf, size_t size) {
    const char* alert = "{\""ALERT_NAME"\" : \"%s\"}";
    const char* alert_start = "{\""ALERT_NAME"\" : \"%s\", \""ALERT_START"\" : %lu}";
    const char* alert_stop = "{\""ALERT_NAME"\" : \"%s\", \""ALERT_START"\" : %lu, \""ALERT_END"\" : %lu}";

    if(!start_date)
        snprintf(buf, size-1, alert, ac_cam_event2string(event));
    else if(!end_date)
        snprintf(buf, size-1, alert_start, ac_cam_event2string(event), start_date);
    else
        snprintf(buf, size-1, alert_stop, ac_cam_event2string(event), start_date, end_date);

    buf[size] = '\0';
    return buf;
}
/*
 * Return camera event or 0 (AC_CAM_EVENT_UNDEF) if snth wrong
 * {"alertName" : "<name>[, "startDate" : <time_t>[, "endDate" : time_t]]}
 */
t_ao_cam_alert ao_cam_decode_alert(const char* in) {
    t_ao_cam_alert ret;
    ret.command_type = AO_ALRT_CAM;
    ret.cam_event = AC_CAM_EVENT_UNDEF;
    ret.start_date = 0;
    ret.end_date = 0;

    cJSON* obj = cJSON_Parse(in);
    if(!obj) return ret;

    cJSON* alrt_name = cJSON_GetObjectItem(obj, ALERT_NAME);
    if(alrt_name) ret.cam_event = ac_cam_string2event(alrt_name->valuestring);

    cJSON* start_date = cJSON_GetObjectItem(obj, ALERT_END);
    if(start_date) ret.start_date = start_date->valueint;

    cJSON* end_date = cJSON_GetObjectItem(obj, ALERT_START);
    if(end_date) ret.end_date = end_date->valueint;

    cJSON_Delete(obj);
    return ret;
}

/************************************************************
   Camera commands
*/
typedef struct {
    int first, first_after;
} pos_t;

/* to take first ptr == lst. Return ptr points to the next position after name value*/
static const char* get_next_param(char* buf, size_t size, const char* ptr) {
    pos_t pos={0,0};
    buf[0] = '\0';
    while(!isalpha(ptr[pos.first]) && (ptr[pos.first] != '\r') && (ptr[pos.first] != '\n') && (ptr[pos.first] != '\0')) pos.first++;
    if(!isalpha(ptr[pos.first])) return NULL;
    pos.first_after = au_findSubstr(ptr+pos.first, "\r\n", AU_CASE);
    if(pos.first_after < 0) return NULL;
    else pos.first_after += pos.first;      /* We start search from pos_first */

    memcpy(buf, ptr+pos.first, (size_t)(pos.first_after-pos.first));
    buf[pos.first_after-pos.first] = '\0';
    return (strlen(buf))?ptr+pos.first_after+2:NULL;    /* +2 skip s\r\n */
}

static par_t str2par_name(const char* buf) {
    par_t i;
    for(i = EP_UNDEFINED+1; i < EP_SIZE; i++) {
        if(!strncmp(PAR_ARRAY[i], buf, strlen(PAR_ARRAY[i]))) return i;
    }
    return EP_UNDEFINED;
}
static void store_rect(int cmd_id,  par_t par_id, const char* buf) {
    if(cmd_id != AO_CAM_CMD_MD) {
        pu_log(LL_ERROR, "%s: Wrong command id %d. %d only allowed. Value in %s not saved", __FUNCTION__, cmd_id, AO_CAM_CMD_MD, buf);
    }
    rect_t *value;
    rect_t v = {0};
    char b[20];
    if(sscanf(buf, "%s%i,%i,%i,%i,%i", b, &v.x0,&v.x1,&v.x2,&v.x3,&v.sensitivity)!= 6) {
        pu_log(LL_ERROR, "%s: Error scanning of %s. Value nit saved", __FUNCTION__, buf);
        return;
    }
    value = calloc(1, sizeof(rect_t));
    if(!value) {
        pu_log(LL_ERROR, "%s Not enough memory", __FUNCTION__);
        return;
    }
    *value = v;

    int i;
    switch (par_id) {
        case EP_RECT0:
            i = 0;
            break;
        case EP_RECT1:
            i = 1;
            break;
        case EP_RECT2:
            i = 2;
            break;
        case EP_RECT3:
            i = 3;
            break;
        default:
            pu_log(LL_ERROR, "%s: Wrong parameter id %d. RECT0-RECT3 expected. Value in %s not saved", __FUNCTION__, par_id, buf);
            free(value);
            return;
    }
    if(MD_PARAMS.rect[i]) free(MD_PARAMS.rect[i]);
    MD_PARAMS.rect[i] = value;
}
static void store_str(int cmd, par_t par_id, const char* buf) {
    if((cmd != AO_CAM_CMD_MD) && (cmd != AO_CAM_CMD_SD)){
        pu_log(LL_ERROR, "%s: Wrong command id %d. %d or %d only allowed. Value in %s not saved", __FUNCTION__, cmd, AO_CAM_CMD_MD, AO_CAM_CMD_SD, buf);
    }
    char* value = NULL;
    int pos = au_findSubstr(buf, " ", AU_CASE);

    if(pos >= 0) {
        char val[128] = {0};
        if (sscanf(buf, "%s", val) == 1) value = strdup(val);
    }
    int i;
    switch (par_id) {
        case EP_TS0:
            i = 0;
            break;
        case EP_TS1:
            i = 1;
            break;
        case EP_TS2:
            i = 2;
            break;
        case EP_TS3:
            i = 3;
            break;
        default:
            pu_log(LL_ERROR, "%s: Wrong parameter id %d. TS0-TS3 expected. Value in %s not saved", __FUNCTION__, par_id, buf);
            if(value) free(value);
            return;
    }
    if(cmd == AO_CAM_CMD_MD) {
        if (MD_PARAMS.ts[i]) free(MD_PARAMS.ts[i]);
        MD_PARAMS.ts[i] = value;
    }
    else if(cmd == AO_CAM_CMD_SD) {
        if (SD_PARAMS.ts[i]) free(SD_PARAMS.ts[i]);
        SD_PARAMS.ts[i] = value;
    }
}
static void store_int(int cmd, par_t par_id, const char* buf) {
    char name[128];
    int val = 0;    /* in case we got just name w/o value at all */
    int ret = sscanf(buf, "%s%i", name, &val);
    if(ret < 2) {
        pu_log(LL_WARNING, "%s: No value found in %s. 0 stored", __FUNCTION__, buf);
    }
//TODO! owful code :-(
    switch(par_id) {
        case EP_RECCH:
            if(cmd == AO_CAM_CMD_MD) MD_PARAMS.recch = val;
            else if(cmd == AO_CAM_CMD_SD) SD_PARAMS.recch = val;
            break;
        case EP_TAPECH:
            if(cmd == AO_CAM_CMD_MD) MD_PARAMS.tapech = val;
            else if(cmd == AO_CAM_CMD_SD) SD_PARAMS.tapech = val;
            break;
        case EP_DEALMODE:
            if(cmd == AO_CAM_CMD_MD) MD_PARAMS.dealmode = val;
            else if(cmd == AO_CAM_CMD_SD) SD_PARAMS.dealmode = val;
            break;
        case EP_ENABLE:
            if(cmd == AO_CAM_CMD_SD) SD_PARAMS.enable = val;
            break;
        case EP_SESETIVITY:
            if(cmd == AO_CAM_CMD_SD) SD_PARAMS.sensitivity = val;
            break;
        case EP_CHN:
            if(cmd == AO_CAM_CMD_MD) MD_PARAMS.chn = val;
            break;
        default:
            pu_log(LL_ERROR, "%s: cmd_id %d and para_id %d from %s not found. Value not stored", __FUNCTION__, cmd, par_id, buf);
            break;
    }
}
static char* make_md_params() {
    char buf[256]={0};
    char rect_buf0[30]={0};
    char ts0_buf[30]={0};
    const char* md_fmt=
            "%s=%d&%s=%d"
            "&%s=%s&%s=&%s=&%s="
            "&%s=%d"
            "&%s=%s&%s=&%s=&%s="
            "&%s=%d";
    const char* rect_fmt="%d,%d,%d,%d, %d";
    if(MD_PARAMS.rect[0]) snprintf(rect_buf0, sizeof(rect_buf0)-1, rect_fmt, MD_PARAMS.rect[0]->x0,MD_PARAMS.rect[0]->x1,MD_PARAMS.rect[0]->x2,MD_PARAMS.rect[0]->x3,MD_PARAMS.rect[0]->sensitivity);
    if(MD_PARAMS.ts[0]) strncpy(ts0_buf, MD_PARAMS.ts[0], sizeof(ts0_buf)-1);
    snprintf(buf, sizeof(buf)-1, md_fmt,
        PAR_ARRAY[EP_RECCH], MD_PARAMS.recch,PAR_ARRAY[EP_TAPECH], MD_PARAMS.tapech,
        PAR_ARRAY[EP_TS0], ts0_buf, PAR_ARRAY[EP_TS1], PAR_ARRAY[EP_TS2], PAR_ARRAY[EP_TS3],
        PAR_ARRAY[EP_DEALMODE], MD_PARAMS.dealmode,
        PAR_ARRAY[EP_RECT0], rect_buf0, PAR_ARRAY[EP_RECT1], PAR_ARRAY[EP_RECT2], PAR_ARRAY[EP_RECT3],
        PAR_ARRAY[EP_CHN], MD_PARAMS.chn
    );
    return strdup(buf);
}
static char* make_sd_params() {
    char buf[256]={0};
    char ts0_buf[30]={0};
    const char* sd_fmt=
            "%s=%d&%s=%d"
            "&%s=%s&%s=&%s=&%s="
            "&%s=%d"
            "&%s=%d&%s=%d";
    if(SD_PARAMS.ts[0]) strncpy(ts0_buf, SD_PARAMS.ts[0], sizeof(ts0_buf)-1);
    snprintf(buf, sizeof(buf)-1, sd_fmt,
             PAR_ARRAY[EP_TAPECH],SD_PARAMS.tapech, PAR_ARRAY[EP_RECCH],SD_PARAMS.recch,
             PAR_ARRAY[EP_TS0],ts0_buf, PAR_ARRAY[EP_TS1], PAR_ARRAY[EP_TS2], PAR_ARRAY[EP_TS3],
             PAR_ARRAY[EP_DEALMODE],SD_PARAMS.dealmode,
             PAR_ARRAY[EP_ENABLE],SD_PARAMS.enable, PAR_ARRAY[EP_SESETIVITY],SD_PARAMS.sensitivity
    );
    return strdup(buf);
}


/****************************************************************************/


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
            name = (read_pars)?CMD_SD_NAME PAR_READ:CMD_SD_NAME;
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
/*
 * Save to local store param from dB
 */
void ao_save_parameter(int cmd_id, user_par_t par_id, int par_value) {
    switch(par_id) {
        case AO_CAM_PAR_MD_SENS:
            if(MD_PARAMS.rect[0]) MD_PARAMS.rect[0]->sensitivity = par_value;
            else {
                pu_log(LL_WARNING, "%s: default rect0 for MD doesn't set! Set default values!", __FUNCTION__);
                rect_t rect = {0,0,999,999, par_value};
                MD_PARAMS.rect[0] = calloc(1, sizeof(rect_t));
                memcpy(MD_PARAMS.rect[0], &rect, sizeof(rect_t));
            }
            break;
        case AO_CAM_PAR_MD_ON:
            if(par_value) {
                if(!MD_PARAMS.ts[0]) {  /* Add ts0 */
                    MD_PARAMS.ts[0] = strdup(PAR_MDSD_ON);
                }
            }
            else if(MD_PARAMS.ts[0]) {  /* Remove ts0 */
                free(MD_PARAMS.ts[0]);
                MD_PARAMS.ts[0] = NULL;
            }
            break;
        case AO_CAM_PAR_SD_SENS:
            SD_PARAMS.sensitivity = par_value;
            break;
        case AO_CAM_PAR_SD_ON:
            if(par_value) {
                if(!SD_PARAMS.ts[0]) {  /* Add ts0 */
                    SD_PARAMS.ts[0] = strdup(PAR_MDSD_ON);
                }
            }
            else if(SD_PARAMS.ts[0]) {  /* Remove ts0 */
                free(SD_PARAMS.ts[0]);
                SD_PARAMS.ts[0] = NULL;
            }
            break;

        default:
            pu_log(LL_ERROR, "%s: Unrecognized user parameter type %d. Not saved", __FUNCTION__, par_id);
            break;
    }
}
/*
 * extract params from lst and save it in local store
 * lst format: name value\r\n...name value\r\n\r\n
 */
void ao_save_params(int cmd_id, const char* lst) {
    char buf[128];
    const char* ptr = lst;
    while(ptr = get_next_param(buf, sizeof(buf), ptr), strlen(buf) != 0) {
        int par_id = str2par_name(buf);
        if(par_id == EP_UNDEFINED) {
            pu_log(LL_ERROR, "%s: parameter %s undefined. Value ignored.", __FUNCTION__, buf);
            continue;
        }
        if((par_id == EP_RECT0)|| (par_id == EP_RECT1)||
            (par_id == EP_RECT2)||(par_id == EP_RECT3)) store_rect(cmd_id, par_id, buf);
        else if((par_id == EP_TS0)|| (par_id == EP_TS1)||(par_id == EP_TS2) ||
            (par_id == EP_TS3)||(par_id == EP_TS3)) store_str(cmd_id, par_id, buf);
        else store_int(cmd_id, par_id, buf);
    }
}
/*
 * create params list from local store and return ub lst
 * NB! lst sould be freed after use!
 */
char* ao_make_params(int cmd_id) {
    switch (cmd_id) {
        case AO_CAM_CMD_MD:
            return make_md_params();
        case AO_CAM_CMD_SD:
            return make_sd_params();
        default:
            pu_log(LL_ERROR, "%s: Command id %d not served.", __FUNCTION__, cmd_id);
            break;
    }
    return NULL;
}

/*
 * Get cmd's parameter
 */
int ao_get_param_value(int cmd_id, user_par_t par_id) {
    int ret = 0;
    switch (par_id) {
        case AO_CAM_PAR_MD_SENS:
            return (MD_PARAMS.rect[0])?MD_PARAMS.rect[0]->sensitivity:0;
        case AO_CAM_PAR_MD_ON:
            return (MD_PARAMS.ts[0] != NULL);
        case AO_CAM_PAR_SD_SENS:
            return SD_PARAMS.sensitivity;
        case AO_CAM_PAR_SD_ON:
            return (SD_PARAMS.ts[0] != NULL);
        default:
            pu_log(LL_ERROR, "%s: Unknown user parameter type %d. 0 returned", __FUNCTION__, par_id);
            break;
    }
    return ret;
}

