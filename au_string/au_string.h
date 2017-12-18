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

#define AU_NOCASE   0
#define AU_CASE     1

char* au_strcpy(char* dest, const char* source, size_t size);
char* au_strcat(char*dest, const char* source, size_t size);

/* Return position in the msg when subs starts. Else return -1. */
int au_findSubstr(const char* msg, const char* subs, int case_sencitive);

/* If msg starts from digit, return substr before furst non-digit. Else return empty str */
const char* au_getNumber(char* buf, size_t size, const char* msg);

const char* au_getSection(char* buf, size_t size, const char* msg, const char* before, const char* after, int case_sencitive);

char* au_replaceSection(char* msg, size_t m_size,  const char* before, const char* after, int caseSencitive, const char* substr);

#endif /* IPCAMTENVIS_AU_STRING_H */
