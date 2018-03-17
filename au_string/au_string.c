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
 Created by gsg on 17/12/17.
 All the garbage regarding trivial string operations
*/
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <malloc.h>

#include "pu_logger.h"

#include "au_string.h"

typedef struct {
    int found;
    int start;
    int len;
} t_au_pos;

static int char_eq(char a, char b, int cs) {
    if(cs) return a == b;
    return tolower(a) == tolower(b);
}
static t_au_pos findSection(const char* msg, const char* before, const char* after, int case_sencitive) {
    t_au_pos ret = {0};
    ssize_t end;

    if(!msg) return ret;
    if(!before)
        ret.start = 0;
    else if(ret.start = au_findSubstr(msg, before, case_sencitive), ret.start < 0)
        return ret;
    ret.start += strlen(before);

    if(!after)
        ret.len = strlen(msg)-ret.start;
    else if(end = au_findSubstr(msg+ret.start, after, case_sencitive), end < 0)
        ret.len = strlen(msg)-ret.start;
    else
        ret.len = end;

    ret.found = 1;
    return ret;
}

char* au_strcpy(char* dest, const char* source, size_t size) {
    assert(dest); assert(source);
    if(!size) return dest;

    if((size-1) < strlen(source)) return NULL;
    memcpy(dest, source, strlen(source)+1);
    return dest;
}
char* au_strcat(char* dest, const char* source, size_t size) {
    assert(dest); assert(source);
    if(!size) return dest;
    if((size-1) < strlen(dest)+strlen(source)) return NULL;
    strcat(dest, source);
    return dest;
}
char* au_bytes_2_hex_str(char* dest, const unsigned char* src, unsigned int src_len, size_t size) {
    assert(dest); assert(src);
    if((!size) || (!src_len)) return dest;

    unsigned int i, len;
    len = 0;
    dest[0] = '\0';
    for(i = 0; i < src_len; i++) {
        char hex[3]={0};
        sprintf(hex, "%x", src[i]);
        if(!au_strcat(dest, hex, size)) return NULL;
        len += strlen(hex);
    }
    if(len+1 > size) return NULL;
    dest[len] = '\0';
    return dest;
}


int au_findSubstr(const char* msg, const char* subs, int case_sencitive) {
    if(!msg || !subs) return -1;
    unsigned int i;
    int gotit = 0;
    for(i = 0; i < strlen(msg); i++) {
        if((strlen(msg)-i) >= strlen(subs)) {
            unsigned j;
            for(j = 0; j < strlen(subs); j++) {
                if(!char_eq(msg[i+j], subs[j], case_sencitive)) {
                    gotit = 0;
                    break;
                }
                gotit = 1;
            }
            if(gotit) return i;
        }
        else return -1;
    }
    return -1;
}
int au_findFirstOutOfSet(const char* msg, const char* set) {
    return (int)strspn(msg, set);
}

const char* au_getNumber(char* buf, size_t size, const char* msg) {
    unsigned int i;
    unsigned int counter = 0;

    assert(buf);

    buf[0] = '\0';
    if(!msg) return buf;

    for(i = 0; i < strlen(msg); i++) {
        if(isdigit(msg[i])) {
            buf[counter++] = msg[i];
        }
        else if(counter) {
            buf[counter] = '\0';
            break;
        }
    }
    return buf;
}
const char* au_getSection(char* buf, size_t size, const char* msg, const char* before, const char* after, int case_sencitive) {
    assert(buf); assert(size);

    t_au_pos pos = findSection(msg, before, after, case_sencitive);
    if(!pos.found) return NULL;
    if((size-1) < pos.len) return NULL;
    memcpy(buf, msg+pos.start, pos.len);
    buf[pos.len] = '\0';
    return buf;
}
char* au_replaceSection(char* msg, size_t m_size,  const char* before, const char* after, int caseSencitive, const char* substr) {
    size_t end;
    char* buf;
    t_au_pos pos = {0};

    assert(msg); assert(m_size); assert(substr);

    pos = findSection(msg, before, after, caseSencitive);
    if(!pos.found) return 0;

    end = strlen(msg)-pos.len+strlen(substr)+1;

    if(buf = calloc(end, 1), !buf) {
        pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
        return NULL;
    }

    memcpy(buf, msg, pos.start); buf[pos.start] = '\0';
    if(!au_strcat(buf, substr, end)) return NULL;
    if(!au_strcat(buf, msg+pos.start+pos.len, end)) return NULL;

    if(!au_strcpy(msg, buf, m_size)) return NULL;
    free(buf);

    return msg;
}



