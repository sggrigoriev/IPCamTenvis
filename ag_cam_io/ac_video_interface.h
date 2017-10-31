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
 Assign proper io functions for streaming video: http or udp
 NB! Do not use r/w functions before the ac_video_set_io() call!
*/

#ifndef IPCAMTENVIS_AC_VIDEO_INTERFACE_H
#define IPCAMTENVIS_AC_VIDEO_INTERFACE_H

#include "ab_ring_bufer.h"
#include "ao_cmd_data.h"


typedef const t_ab_block (*t_ac_video_read)(unsigned long);
typedef int(*t_ac_video_write)(size_t, const t_ab_byte*);

typedef int (*t_ac_init_connections)(t_ao_video_start);
typedef int (*t_ac_close_connections)();

/* Set video stream read/write functions depending on configuration
 */
void ac_video_set_io();

t_ac_init_connections ac_init_connections = NULL;
t_ac_close_connections ac_close_connections = NULL;
t_ac_video_read ac_video_read = NULL;
t_ac_video_write ac_video_write = NULL;


#endif /* IPCAMTENVIS_AC_VIDEO_INTERFACE_H */
