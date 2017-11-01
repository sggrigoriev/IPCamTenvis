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

#define AC_MAX_STREAM_BUFF_SIZE 16384

typedef enum {AC_READ_CONN, AC_WRITE_CONN, AC_ALL_CONN} t_ac_conn_type;
/*****************************************************************
 * Read streaming data
 * size_t - buffer size in bytes
 * t_ab_byte - buffer to store info
 * Return 0 if error or amount of bytes red if > 0
 */
typedef size_t (*t_ac_video_read)(size_t, t_ab_byte*);
/******************************************************************
 * Write streaming data
 * size_t - buffer size in bytes
 * t_ab_byte* - buffer with data to be written
 * Return 0 if error, 1 if not
 */
typedef int(*t_ac_video_write)(size_t, const t_ab_byte*);
/**********************************************************************
 * Initiate streaming read/wrie connection
 * t_ao_video_start - connection parameters
 * t_ac_conn_type - AC_READ_CONN for read initiation or AC_WRITE_CONN for write initiation
 * Return 0 if error, 1 if OK
 */
typedef int (*t_ac_init_connections)(t_ao_video_start, t_ac_conn_type);
/*********************************************************************
 * Cllose read or write connection
 * t_ac_conn_type - shows which connection should be closed
 */
typedef int (*t_ac_close_connections)(t_ac_conn_type);

/* Set video stream read/write functions depending on configuration
 */
void ac_video_set_io();

extern t_ac_init_connections ac_init_connections;
extern t_ac_close_connections ac_close_connections;
extern t_ac_video_read ac_video_read;
extern t_ac_video_write ac_video_write;


#endif /* IPCAMTENVIS_AC_VIDEO_INTERFACE_H */
