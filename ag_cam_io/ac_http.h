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
 Created by gsg on 21/10/17.
 Camera HTTP intervace for open, read and write
*/

#ifndef IPCAMTENVIS_AC_HTTP_H
#define IPCAMTENVIS_AC_HTTP_H

#include <stddef.h>

#include "ac_video_interface.h"
/**************************************
 * (Re)connects to the camera
 * @return 1 if connected, 0 if not NB! 0 is case of hard error!!!
 */
int ac_http_reconnect();

/* Read into in_buf the message from the cloud (GET). Answer ACK(s) id command(s) came
 *      in_buf  - buffer for data received
 *      size    - buffer size
 *  Return  0 if timeout, 1 if OK, -1 if error - reconnect required
*/
int ac_http_read(char* in_buf, size_t size);

/* POST the data to cloud; receive (possibly) the answer
 *      buf         - message to be sent (0-terminated string)
 *      resp        - buffer for cloud respond
 *      rest_size   - buffer size
 *  Returns 0 if error, 1 if OK
*/
int ac_http_write(const char* buf, char* resp, size_t resp_size);

int ac_init_http_stream_connections(t_ao_video_start params, t_ac_conn_type rw);
int ac_close_http_sream_connections(t_ac_conn_type rw);

size_t ac_http_stream_read(size_t size, t_ab_byte* buf);
int ac_http_stream_write(size_t size, const t_ab_byte* buf);


#endif /* IPCAMTENVIS_AC_HTTP_H */
