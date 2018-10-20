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

char* au_strcpy(char* dest, const char* source, size_t size) {
    if((!size) || (!source) || (!dest)) return dest;

    if((size-1) < strlen(source)) return dest;
    memcpy(dest, source, strlen(source)+1);
    return dest;
}
char* au_strcat(char* dest, const char* source, size_t size) {
    if((!size) || (!source) || (!dest)) return dest;

    if((size-1) < strlen(dest)+strlen(source)) return dest;
    strcat(dest, source);
    return dest;
}
char* au_strdup(const char* source) {
    if(!source) return NULL;
    char* ret = calloc(strlen(source)+1, 1);

    return strcpy(ret, source);
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

    if(!buf) return buf;

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
/*
 * concat dest and src.
 * NB! the result should be freed after use!
 * IF dest == NULL -> just strdup(src)
 */
char* au_append_str(char* dest, char* src) {
    if(!src) return NULL;
    if(!dest) return strdup(src);

    char* ret = calloc(strlen(dest)+strlen(src)+1, 1);
    if(!ret) {
        pu_log(LL_ERROR, "%s: Not enough memory", __FUNCTION__);
        return NULL;
    }
    sprintf(ret, "%s%s", dest, src);
    return ret;
}
/*
 * Take off last symbol
 * Required for strings as name, name, ... , name
 *
 */
char* au_drop_last_symbol(char* str) {
    if((str) && (strlen(str)))
        str[strlen(str)-1] = '\0';
    return str;
}



