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
 Created by gsg on 30/10/17.
*/

#include "ao_cmd_cloud.h"

t_ao_cloud_msg_type ao_cloud_decode(const char* cloud_message, t_ao_cloud_msg* data) {
    return AO_CLOUD_UNDEF;
}

const char* ao_cloud_encode(const t_ao_cam_msg data, char* cloud_message, size_t msg_size) {
    return NULL;
}
