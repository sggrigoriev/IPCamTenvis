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

#ifndef IPCAMTENVIS_AC_UDP_H
#define IPCAMTENVIS_AC_UDP_H

#include "ac_video_interface.h"

t_ac_init_connections ac_init_udp_connections;
t_ac_close_connections ac_close_udp_connections;

t_ac_video_read ac_udp_stream_read;
t_ac_video_write ac_udp_stream_write;

#endif /* IPCAMTENVIS_AC_UDP_H */