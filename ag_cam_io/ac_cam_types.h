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
 Created by gsg on 10/01/18.
*/

#ifndef IPCAMTENVIS_AC_CAM_TYPES_H
#define IPCAMTENVIS_AC_CAM_TYPES_H

#define AC_PLAYER_VIDEO_PORT 1935
#define AC_FAKE_CLIENT_PORT 11038

#define AC_RTSP_MAX_URL_SIZE        4097
#define AC_RTSP_HEADER_SIZE         4097
#define AC_RTSP_BODY_SIZE           8193
#define AC_RTSP_TRANSPORT_SIZE      100
#define AC_RTSP_SESSION_ID_SIZE     20
#define AC_WOWZA_SESSION_ID_SIZE    30
#define AC_RTSP_TRACK_SIZE          20

#define AC_RTSP_CLIENT_NAME "IOT Proxy"
#define AC_RTSP_CONTENT_TYPE "application/sdp"

#define AC_TRACK            "trackID="
#define AC_SESSION          "Session: "
#define AC_TIMEOUT          "timeout="
#define AC_PLAY_RANGE       "npt=0.000-"
#define AC_VIDEO_TRACK      "0"
#define AC_STREAMING_TCP    1
#define AC_STREAMING_UDP    0
#define AC_RTSP_HEAD        "rtsp://"

#define AC_RTSP_CHALLENGE_HEAD   "WWW-Authenticate: "
#define AC_RTSP_F_DIGEST_REALM  "Digest realm=\""
#define AC_RTSP_F_DIGEST_NONCE  "nonce=\""

#define AC_RTSP_DIGEST_AUTH     "Authorization: "
#define AC_RTSP_DIGEST_UNAME    "Digest username="
#define AC_RTSP_DIGEST_REALM    "realm="
#define AC_RTSP_DIGEST_NONCE    "nonce="
#define AC_RTSP_DIGEST_URI      "uri="
#define AC_RTSP_DIGEST_RESPONSE "response="
#define AC_RTSP_DIGEST_METHOD   "POST"

#define AC_RTSP_EOL         "\r\n"
#define AC_RTSP_SDP_ORIGIN  AC_RTSP_EOL"o="
#define AC_RTSP_SDP_CD      AC_RTSP_EOL"c="
#define AC_RTSP_VS_ORIGIN   "- 0 0 IN IP4 127.0.0.1"
#define AC_RTSP_CD_IP4      "IP4 "

typedef enum {
    AC_CAMERA,
    AC_WOWZA
} t_ac_rtsp_device;

typedef enum {
    AC_STATE_UNDEF,
    AC_STATE_CONNECT,
    AC_STATE_DESCRIBE,
    AC_STATE_SETUP,
    AC_STATE_START_PLAY,
    AC_STATE_PLAYING,
    AC_STATE_STOP_PLAY,
    AC_STATE_ON_ERROR
} t_ac_rtsp_states;

typedef struct _ACRTSPSession {
    t_ac_rtsp_device device;
    t_ac_rtsp_states state;
    char* url;
    char* rtsp_session_id;
    int CSeq;                   /* NB! this is NEXT number */
    int video_port;
    int audio_port;
/* Implementation-specific */
    void* session;
} t_at_rtsp_session;

#define AT_DT_RT(a,b,c)    if(a != b) { \
                                pu_log(LL_ERROR, "%s: Session with wrong device type %d. Device type %d expected", __FUNCTION__, a, b); \
                                return c; \
                            }
#define AT_DT_NR(a,b)       if(a != b) { \
                                pu_log(LL_ERROR, "%s: Session with wrong device type %d. Device type %d expected", __FUNCTION__, a, b); \
                                return; \
                            }

#endif /* IPCAMTENVIS_AC_CAM_TYPES_H */
