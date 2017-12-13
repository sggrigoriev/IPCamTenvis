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
 Created by gsg on 12/12/17.
*/

#ifndef IPCAMTENVIS_AC_HTTP_H
#define IPCAMTENVIS_AC_HTTP_H

#define AC_HTTP_RC_RETRY    -1
#define AC_HTTP_RC_ERROR    0
#define AC_HTTP_RC_OK       1
#define AC_HTTP_REPEATS     3

#include <curl/curl.h>

typedef struct {
    char *buf;
    size_t sz;
} t_ac_callback_buf;

typedef struct {
    CURLSH* h;
    t_ac_callback_buf wr_buf;
    struct curl_slist* slist;
} t_ac_http_handler;

int ac_http_init();
void ac_http_close();

t_ac_http_handler* ac_http_prepare_get_conn(const char* url_string, const char* auth_string);
/* -1 - retry, 0 - error, 1 - OK */
int ac_perform_get_conn(t_ac_http_handler* h, char* answer, size_t size);

void ac_http_close_conn(t_ac_http_handler* h);

#endif /* IPCAMTENVIS_AC_HTTP_H */
