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
 Local cURL wrapper. Has to be joined with libhttp sometime,
*/

#ifndef IPCAMTENVIS_AC_HTTP_H
#define IPCAMTENVIS_AC_HTTP_H

#define AC_HTTP_RC_RETRY    -1
#define AC_HTTP_RC_ERROR    0
#define AC_HTTP_RC_OK       1
#define AC_HTTP_REPEATS     3

#define AC_HTTP_UNAUTH      401

#include <curl/curl.h>

typedef struct {
    char *buf;
    size_t free_space;
    size_t buf_sz;
} t_ac_callback_buf;

typedef struct {
    CURLSH* h;
    t_ac_callback_buf wr_buf;
    char err_buf[CURL_ERROR_SIZE];
    struct curl_slist* slist;
} t_ac_http_handler;
/**
 * Currently it is empty. Obsolete.
 * @return  - 1
 */
int ac_http_init();

/**
 * Also clean & empty. Obsolete.
 */
void ac_http_close();

/**
 * Prepare cURL GET
 *
 * @param url_string    - URL for GET
 * @param auth_string   - auth token (session ID in our case)
 * @return  - poiner to ready handler, NULL if error
 */
t_ac_http_handler* ac_http_prepare_get_conn(const char* url_string, const char* auth_string);
/**
 * Run prepared GET
 * @param h         - poiner to the handler
 * @param answer    - aswer came from GET
 * @param size      - buffer size
 * @return  -   -1 - retry, 0 - error, 1 - OK (see AC_HTTP_RC_*)
 */
int ac_perform_get_conn(t_ac_http_handler* h, char* answer, size_t size);

/**
 * Close the connection, delete memory.
 * @param h - pointer to the handler
 */
void ac_http_close_conn(t_ac_http_handler* h);

/* return CurlErrno */
/**
 * Get cURL errno. Service function.
 *
 * @param perform_rc    - rc returned by cURL
 * @param handler       - pointer to the connection handler
 * @param function      - function returned the rc
 * @return  - returned errno
 */
long ac_http_analyze_perform(CURLcode perform_rc, CURLSH* handler, const char* function);

#endif /* IPCAMTENVIS_AC_HTTP_H */
