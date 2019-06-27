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
 String primitives. Has to pe shifted to common libs after redesign.
*/

#ifndef IPCAMTENVIS_AU_STRING_H
#define IPCAMTENVIS_AU_STRING_H

#include <stddef.h>

#define AU_NOCASE   0
#define AU_CASE     1

/* Calculate max(a,b) */
#define AU_MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

/* Calculate min(a,b) */
#define AU_MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


/**
 * Super-strings funtion process - several strings in one array
 * Control for NULL and size addad
 */
char* au_strcpy(char* dest, const char* source, size_t size);
char* au_strcat(char* dest, const char* source, size_t size);
char* au_strdup(const char* source);

/**
 * Find first substring entry
 *
 * @param msg               - source string
 * @param subs              - subs
 * @param case_sencitive    - 0 no case compare, 1 case compare
 * @return  - position in the msg when subs starts. Else return -1.
 */
int au_findSubstr(const char* msg, const char* subs, int case_sencitive);

/**
 * Find the first entry any char from the set in the string
 *
 * @param msg   - source message
 * @param set   - set of char to find
 * @return  - first position out of chars set, -1 if nothing found
 */
int au_findFirstOutOfSet(const char* msg, const char* set);

/**
 * If msg starts from digit, return substr before first non-digit. Else return empty str
 *
 * @param buf   - buffer for the substring
 * @param size  - buffer size
 * @param msg   - the source msg
 * @return  - substring found or empty string
 */
const char* au_getNumber(char* buf, size_t size, const char* msg);

/**
 * Concat dest + src.
 * NB-1! Dest frees!!
 * NB-2! the result should be freed after use!
 * IF dest == NULL -> just strdup(src)
 *
 * @param dest - result string
 * @param src - initial string
 */
char* au_append_str(char* dest, const char* src);

/**
 * Take off last symbol
 * Required for strings as name, name, ... , name
 *
 * @param str   - initial string
 * @return  - pointer to the str
 */
char* au_drop_last_symbol(char* str);

/**
 * Take size-1 symbols. If *name got less - return 0
 * Take size-1 symbols from **name, shift pointer, copy symbols to buf.
 *
 * @param name  - pointer to the string !modified after function use!
 * @param buf   - bufer to copy size-1 symbols
 * @param size  - amount of chars to extract
 * @return  - 0 if op failed, 1 - if Ok
 */
int au_getNsyms(const char** name, char* buf, size_t size);

/**
 * Take n digits from *name, convert it to nimber.
 *
 * @param name  - pointer to the string
 * @param res   - ponter to the converted integer
 * @param n     - amount of digits to extract
 * @return  - 0 if name doesn't have n digits in a row
 */
int au_getNdigs(const char** name,  int* res, int n);

/**
 * Get syms until the delimiter.
 *
 * @param name  - pointer to the initial string !Modified!
 * @param buf   - buffer for result
 * @param size  - buffer size
 * @param delim - delimiter
 * @return  - 0 if syms amt greater than size-1, 1 if Ok
 */
int au_getUntil(const char** name, char* buf, size_t size, char delim);

/**
 * Copy file path to path and file name to name.
 * Copy empty str if not found.
 *
 * @param nameNpath - initial string with path and file name
 * @param path      - buffer to save the path
 * @param psize     - buffer size
 * @param name      - buffer to save the name
 * @param nsize     - buffer size
 */
void au_splitNamePath(const char* nameNpath, char* path, size_t psize, char* name, size_t nsize);

#endif /* IPCAMTENVIS_AU_STRING_H */