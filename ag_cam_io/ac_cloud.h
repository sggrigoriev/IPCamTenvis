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
 Support retrieving video parameters and WS interface parameters from the cloud
 It is some workflow inside.
*/

#ifndef IPCAMTENVIS_AC_CLOUD_H
#define IPCAMTENVIS_AC_CLOUD_H

#include <stdlib.h>

/**
 * Get camera connection parameters from the cloud
 *
 * @param v_url     - URL for streaming into Wowza
 * @param v_size    - buffer size
 * @param v_port    - Wowza port
 * @param v_sess    - session ID
 * @param vs_size   - buffer size
 * @param w_url     - WEB socket url (Cam manages by the cloud with separate Web Socket interface!)
 * @param w_size    - buffer size
 * @param w_port    - WEB socket port
 * @param w_sess    - WEB socket session ID
 * @param ws_size   - buffer size
 * @return  - 0 if errorm 1 if Ok
 */
int ac_cloud_get_params(char* v_url, size_t v_size, int* v_port, char* v_sess, size_t vs_size, char* w_url, size_t w_size, int* w_port, char* w_sess, size_t ws_size);

#endif /* IPCAMTENVIS_AC_CLOUD_H */
