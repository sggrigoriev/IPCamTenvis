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
 Created by gsg on 09/07/18.
*/

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "pu_logger.h"

#include "au_string.h"
#include "ac_cam_types.h"

#define CONTROL         "control"
#define COMMON_CONTROL  "a=control:*\r\n"
#define VIDEO_TRACK     "trackID=0"
#define AUDIO_TRACK     "trackID=1"

#define O_STRING        "- 0 0 IN IP4 127.0.0.1"
#define DEST_IP_RPEF    "c=IN IP4 "
#define CONTROL_PREF    "a=control:"


#define NO_CASE_SENS        0
#define CASE_SENS           1
#define SDP_END_STR     "\r\n"

#define SDP_HEADRES     "vosctam"

#define AT_MEM_ERR() {pu_log(LL_ERROR, "Memory allocation error in %s at %d", __FUNCTION__, __LINE__); goto on_error;}

typedef enum {AC_COMMON, AC_VIDEO, AC_AUDIO} t_sdp_section;

typedef struct {
    const char* next_str;    /* Points on the next non-parsed SDP string */
    char curr_str[200];    /* Buffer with the current parsed string */
    t_sdp_section section;
    char str_name;          /* SDP string defenitoe (a, c, m, ... */
    const char* key_name;   /* first token after '=' */
    const char* key_value;  /* token afer ":" or null if ':' not found*/
} t_sdp_read_desr;

/*
 * Games around SDP
 */
/*
 * find '<char>=', return <char> set poa on next after '='
 */
static char get_sdp_str_name(const char* str, int* pos) {
    int i = 0;
    while (!isalpha(str[i]) && (str[i] != '\0')) i++;
    if((str[i] == '\0') || str[i+1] != '=') return ' ';
    *pos = i+2;
    return str[i];
}
/*
 * smth finished by ':' or ' ' or \r\n (CRLF)
 */
static const char* get_sdp_key_name(const char* str, int* pos) {
    *pos = au_findSubstr(str, ":", NO_CASE_SENS);
    if(*pos > 0) return str;
    *pos = au_findSubstr(str, " ", NO_CASE_SENS);
    if(*pos > 0) return str;
    *pos = au_findSubstr(str, SDP_END_STR, NO_CASE_SENS);
    return str;
}
static int parse_sdp_string(t_sdp_read_desr* sdp) {
/* Parse current string */
    int pos = 0;
    char* parsed_string = sdp->curr_str;
    sdp->str_name = get_sdp_str_name(parsed_string, &pos); parsed_string += pos; pos = 0;
    if(sdp->str_name == ' ') {
        pu_log(LL_ERROR, "%s: SDP string name not found in %s", __FUNCTION__, sdp->curr_str);
        return 0;
    }
    sdp->key_name = NULL;
    sdp->key_value = NULL;

    switch(sdp->str_name) {
        case 'v':
        case 'o':
        case 's':
        case 'c':
        case 't':   /* No need for further parsing */
            break;
        case 'a':   /* Extract name & key if exists */
            sdp->key_name = get_sdp_key_name(parsed_string, &pos); parsed_string += pos; pos = 0;
            if(parsed_string[0] != ':') {   /* no value */
                sdp->key_value = NULL;
            }
            else {
                parsed_string++;
                sdp->key_value = parsed_string;
            }
            break;
        case 'm': {   /* Get section name */
            char buf[10]={0};
            pos = au_findSubstr(parsed_string, " ", NO_CASE_SENS);
            if(pos < 0) {
                pu_log(LL_ERROR, "%s: Section name not found in %s", __FUNCTION__, sdp->curr_str);
                return 0;
            }
            memcpy(buf, parsed_string, (size_t)pos);
            buf[pos] = '\0';
            if(!strcmp(buf, "video")) sdp->section = AC_VIDEO;
            else if(!strcmp(buf, "audio")) sdp->section = AC_AUDIO;
            else {
                pu_log(LL_ERROR, "%s: Section name not found in %s", __FUNCTION__, sdp->curr_str);
                return 0;
            }
        }
            break;
        default:
            break;
    }
    return 1;
}
static t_sdp_read_desr* get_next(t_sdp_read_desr* sdp) {
    int pos;

/* Move on SDP string */
    pos = au_findSubstr(sdp->next_str, SDP_END_STR, NO_CASE_SENS);
    if(pos <= 0) return NULL;    /* SDP ends */
    memcpy(sdp->curr_str, sdp->next_str, pos+strlen(SDP_END_STR));
    sdp->curr_str[pos+strlen(SDP_END_STR)] = '\0';
    sdp->next_str += pos + strlen(SDP_END_STR);

    if(!parse_sdp_string(sdp)) return NULL;

    return sdp;
}

static t_sdp_read_desr* open_sdp(const char* text_sdp) {
    t_sdp_read_desr* ret = calloc(sizeof(t_sdp_read_desr), 1);
    if(!ret) AT_MEM_ERR();

    ret->section = AC_COMMON;
    int pos = au_findSubstr(text_sdp, SDP_END_STR, NO_CASE_SENS);   /* Get the first string */
    memcpy(ret->curr_str, text_sdp, pos+strlen(SDP_END_STR)+1);
    ret->curr_str[pos+strlen(SDP_END_STR)] = '\0';
    ret->next_str = text_sdp + pos + strlen(SDP_END_STR);
    if(!parse_sdp_string(ret)) return NULL;

    return ret;
on_error:
    if(ret) free(ret);
    return NULL;
}
static void close_sdp(t_sdp_read_desr* sdp) {
    if(sdp) free(sdp);
}
/*
 * Replace the o= value to - 0 0 IN IP4 127.0.0.1
 * or do nothing
 */

static void replace_o(char* buf, size_t size, t_sdp_read_desr* sdp) {
    if(sdp->str_name != 'o') return;
    int pos = au_findSubstr(sdp->curr_str, "=", NO_CASE_SENS);
    memcpy(buf, sdp->curr_str, pos+1);
    buf[pos+1] = '\0';
    strncat(buf, O_STRING, size-strlen(buf)-1);
    strncat(buf, SDP_END_STR, size - strlen(buf)-1);
}
/*
 * Replace the "c=IN IP4 <IP_number spec> to <control_url>
 * or do nothing
 */
static void replace_ip(char* buf, size_t size, t_sdp_read_desr* sdp, const char* control_url) {
    int pos = au_findSubstr(sdp->curr_str, DEST_IP_RPEF, NO_CASE_SENS);
    if(pos < 0) return;
    memcpy(buf, sdp->curr_str, strlen(DEST_IP_RPEF)+pos);
    buf[strlen(DEST_IP_RPEF)+pos] = '\0';
    strncat(buf, control_url, size-strlen(buf)-1);
    strncat(buf, SDP_END_STR, size - strlen(buf)-1);
}
/*
 * replace control: value if sdp contans the control or do nothing
 * a=control:
 */
static void replace_control_value(char* buf, size_t size, t_sdp_read_desr* sdp, const char* replacement) {
    int pos = au_findSubstr(sdp->curr_str, CONTROL_PREF, NO_CASE_SENS);
    if(pos < 0) return;
    memcpy(buf, sdp->curr_str, strlen(CONTROL_PREF)+pos);
    buf[strlen(CONTROL_PREF)+pos] = '\0';
    strncat(buf, replacement, size-strlen(buf)-1);
    strncat(buf, SDP_END_STR, size-strlen(buf)-1);
}
/*
 * add the_string to the buf, return new buf length
 */
size_t static add_string(char* buf, size_t size, const char* the_string) {
    strncat(buf, the_string, size);
    return size - strlen(buf);
}
/*
 * Copy whole string until \r\n, including'em
 */
static char* copy_string(char* buf, size_t size, t_sdp_read_desr* sdp) {
    int pos = au_findSubstr(sdp->curr_str, SDP_END_STR, NO_CASE_SENS);
    if(pos < 0) return buf;
    memcpy(buf, sdp->curr_str, pos+strlen(SDP_END_STR));
    buf[pos+strlen(SDP_END_STR)+1] = '\0';
    return buf;
}
/*****************************************************
 * Prepares the sdp to be sent to Wowza:
 * 1 Replace:
   From:
   o=StreamingServer <sess_id> <sess_smth> IN IP4 <cam_ip>
   to
   o=- 0 0 IN IP4 127.0.0.1
2 Replace:
   From:
   c=IN IP4 <cam_ip>
   to:
   c=IN IP4 <Wowza IP>
3 Add
   a=control: *
4 Replace for video (if "m=video" found)
   From:
   a=control:<cam setup video url>
   to:
   a=control:trackID=0
5 Replace for audio (if "m=audio" found)
   From:
   a=control:<cam setup audio url>
   to:
   a=control:trackID=1
 * @param new_sdp - changed SDP
 * @param size - buf size
 * @param old_sdp  - cam's SDP
 * @param control_url - Wowza IP for #2 Replace
 * @return 1 if OK, 0 if not
*/
int ac_rtsp_make_announce_body(char* new_sdp, size_t size, const char* old_sdp, const char* control_url) {
    int rc = 0;

    t_sdp_read_desr* sdp = open_sdp(old_sdp);
    if(!sdp) {
        pu_log(LL_ERROR, "%s: Error start SDP %s parsing", __FUNCTION__, old_sdp);
        goto on_error;
    }
    bzero(new_sdp, size);
    int common_control_inserted = 0;
    t_sdp_read_desr* tmp = sdp;
    do {
        char sdp_string[100]={0};
        sdp = tmp;
        switch (sdp->section) {
            case AC_COMMON:
                replace_o(sdp_string, sizeof(sdp_string), sdp);
                replace_ip(sdp_string, sizeof(sdp_string), sdp, control_url);
                break;
            case AC_AUDIO:
                if(!common_control_inserted) {
                    common_control_inserted = 1;
                    size = add_string(new_sdp, size, COMMON_CONTROL);
                }
                replace_control_value(sdp_string, sizeof(sdp_string), sdp, AUDIO_TRACK);
                break;
            case AC_VIDEO:
                if(!common_control_inserted) {
                    common_control_inserted = 1;
                    size = add_string(new_sdp, size, COMMON_CONTROL);
                }
                replace_control_value(sdp_string, sizeof(sdp_string), sdp, VIDEO_TRACK);
                break;
            default:
                copy_string(sdp_string, sizeof(sdp_string), sdp);
                break;
        }
        if(strchr(SDP_HEADRES, sdp->str_name) != NULL) {
            if(!strlen(sdp_string)) copy_string(sdp_string, sizeof(sdp_string), sdp);
            au_strcat(new_sdp, sdp_string, size);
            size -= strlen(sdp_string);
        }

    }
    while(tmp = get_next(sdp), tmp != NULL);
    au_strcat(new_sdp, SDP_END_STR, size);  /* add last \r\n */
    rc = 1;
    close_sdp(sdp);
on_error:
    return rc;
}

/**************************************************
 * Setup sess->audie/video _url use audio/media "a=control": values
 * Add values to the session url or replace session url for audio/video setups
 * @param sdp - Wowze/AP SDP
 * @param sess - Wowza/AP sesion descriptor
 * @param is_replace - 1 - make replacement,0 - concatinate
 * @return - 1 if OK, 0 if not
 */
int ac_rtsp_set_setup_urls(const char* text_sdp, t_at_rtsp_session* sess, int is_replace) {
    int rc = 0;

    t_sdp_read_desr* sdp = open_sdp(text_sdp);
    if(!sdp) {
        pu_log(LL_ERROR, "%s: Error start SDP %s parsing", __FUNCTION__, text_sdp);
        goto on_error;
    }
    t_sdp_read_desr* tmp = sdp;
    do {
        sdp = tmp;
        if((sdp->section != AC_COMMON) && (sdp->key_name != NULL) && (strstr(sdp->key_name, CONTROL) != NULL)) { /* means control on Video or Audio section */
            char** url;
            if(sdp->section == AC_AUDIO)
                url = &sess->audio_url;
            else if(sdp->section == AC_VIDEO)
                url = &sess->video_url;
            else {
                pu_log(LL_ERROR, "%s: unrecognozed section type %d in SDP %s", __FUNCTION__, sdp->section, text_sdp);
                goto on_error;
            }
            char buf[100]={0};
            int pos = au_findSubstr(sdp->key_value, SDP_END_STR, NO_CASE_SENS);
            memcpy(buf, sdp->key_value, pos+strlen(SDP_END_STR));
            buf[pos+strlen(SDP_END_STR)] = '\0';
            if(is_replace)
                *url = au_strdup(buf);
            else {
                char bbuf[AC_RTSP_MAX_URL_SIZE] = {0};
                snprintf(bbuf, sizeof(bbuf)-1, "%s/%s", sess->url, buf);
                if(*url = au_strdup(bbuf), !url) AT_MEM_ERR();
            }
        }
    }
    while(tmp = get_next(sdp), tmp != NULL);
    rc = 1;
    close_sdp(sdp);
on_error:
    return rc;
}

