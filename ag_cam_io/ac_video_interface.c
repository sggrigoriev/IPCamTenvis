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

#include "pu_logger.h"

#include "ag_defaults.h"
#include "ag_settings.h"

#include "ac_http.h"
#include "ac_udp.h"

#include "ac_video_interface.h"

t_ac_init_connections ac_init_connections = NULL;
t_ac_close_connections ac_close_connections = NULL;
t_ac_video_read ac_video_read = NULL;
t_ac_video_write ac_video_write = NULL;


void ac_video_set_io() {
    int code = ag_getIPCamProtocol();
    switch(code) {
        case AG_VIDEO_RTMP:
            pu_log(LL_INFO, "The RTMP interface is assigned");
            ac_video_read = &ac_http_stream_read;
            ac_video_write = &ac_http_stream_write;
            ac_init_connections = &ac_init_http_stream_connections;
            ac_close_connections = ac_close_http_sream_connections;
            break;
        case AG_VIDEO_RTSP:
            pu_log(LL_INFO, "The RTSP interface is assigned");
            ac_video_read = &ac_udp_stream_read;
            ac_video_write = &ac_udp_stream_write;
            ac_init_connections = &ac_init_http_stream_connections;
            ac_close_connections = &ac_close_http_sream_connections;
            break;
        default:
            pu_log(LL_ERROR, "Unsupported interface code %d. RTMP will be used instead", code);
            ac_video_read = &ac_http_stream_read;
            ac_video_write = &ac_http_stream_write;
            ac_init_connections = &ac_init_http_stream_connections;
            ac_close_connections = &ac_close_http_sream_connections;
            break;
    }
}


