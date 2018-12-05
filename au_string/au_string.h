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
*/

#ifndef IPCAMTENVIS_AU_STRING_H
#define IPCAMTENVIS_AU_STRING_H

#include <stddef.h>

#define AU_NOCASE   0
#define AU_CASE     1

#define AU_MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define AU_MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


/*
 * Super-strings funtion process - several strings in one array
 */

char* au_strcpy(char* dest, const char* source, size_t size);
char* au_strcat(char* dest, const char* source, size_t size);
char* au_strdup(const char* source);

/* Return position in the msg when subs starts. Else return -1. */
int au_findSubstr(const char* msg, const char* subs, int case_sencitive);

/* Return first position out of chars set */
int au_findFirstOutOfSet(const char* msg, const char* set);

/* If msg starts from digit, return substr before furst non-digit. Else return empty str */
const char* au_getNumber(char* buf, size_t size, const char* msg);
/*
 * concat dest and src. dest frees!!
 * NB! the result should be freed after use!
 * IF dest == NULL -> just strdup(src)
 */
char* au_append_str(char* dest, const char* src);
/*
 * Take off last symbol
 * Required for strings as name, name, ... , name
 *
 */
char* au_drop_last_symbol(char* str);
/*
 * take size-1 sumbols. is *name got less - return 0
 * take size-1 symbols from **name, shift pointer, copy symbols to buf.
 */
int au_getNsyms(const char** name, char* buf, size_t size);
/*
 * Take n digits from *name, convert it to nimber.
 * Return 0 if name doesn't have n digits in a row
 */
int au_getNdigs(const char** name,  int* res, int n);
/*
 * Get syms until the delimeter.
 * Return 0 if syms amt greater than size-1
 */
int au_getUntil(const char** name, char* buf, size_t size, char delim);

#endif /* IPCAMTENVIS_AU_STRING_H */
