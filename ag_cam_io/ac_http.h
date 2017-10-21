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

/**************************************
 * (Re)connects to the camera
 * @return 1 if connected, 0 if not NB! 0 is case of hard error!!!
 */
int ac_reconnect();

/* Read into in_buf the message from the cloud (GET). Answer ACK(s) id command(s) came
 *      in_buf  - buffer for data received
 *      size    - buffer size
 *  Return  0 if timeout, 1 if OK, -1 if error - reconnect required
*/
int ac_read(char* in_buf, size_t size);

/* POST the data to cloud; receive (possibly) the answer
 *      buf         - message to be sent (0-terminated string)
 *      resp        - buffer for cloud respond
 *      rest_size   - buffer size
 *  Returns 0 if error, 1 if OK
*/
int ac_write(char* buf, char* resp, size_t resp_size);


#endif /* IPCAMTENVIS_AC_HTTP_H */
