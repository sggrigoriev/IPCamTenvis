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

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/rtsp/gstrtsp.h>
#include <gst/rtsp/gstrtspurl.h>
#include <gst/rtsp/gstrtspconnection.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <gst/sdp/gstsdpmessage.h>

#include "pu_logger.h"

#include "ag_defaults.h"
#include "ag_settings.h"
#include "au_string.h"
#include "rtsp_msg_ext.h"

#include "ac_wowza.h"
#include "ac_cam_types.h"

#define AC_GST_ANAL(a)    if((a) != GST_RTSP_OK) {\
                                gchar* res = gst_rtsp_strresult(a);  \
                                pu_log(LL_ERROR, "%s: GST reports error %s", __FUNCTION__, res); \
                                g_free(res); \
                                goto on_error; \
                            }

typedef struct {
    GstRTSPUrl* url;
    char* wowza_session;
    GstRTSPConnection *conn;
    GTimeVal io_to;
}t_gst_session;

/*
 * AA-XX-YY-ZZZ
 * AA - file.c - 8 for Wowza
 * YY - function #
 * ZZZ - line number
*/

extern volatile uint32_t contextId;

/* 00 */
static void clear_session(t_gst_session* gs) {
    if(gs) {
        if(gs->wowza_session) free(gs->wowza_session);
        if(gs->conn) {
            gst_rtsp_connection_close(gs->conn);
            gst_rtsp_connection_free(gs->conn);
        }
        if(gs->url) gst_rtsp_url_free(gs->url);
        free(gs);
    }
}

/* 06 */
int ac_WowzaInit(t_at_rtsp_session* sess, const char* wowza_session_id) {
    pu_log(LL_DEBUG, "%s start", __FUNCTION__);
    AT_DT_RT(sess->device, AC_WOWZA, 0);

    GstRTSPResult rc;
    GTimeVal conn_timeout = {10,0};

    t_gst_session* gs = NULL;
    if(gs = calloc(sizeof(t_gst_session), 1), !gs) {
        pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    rc = gst_rtsp_url_parse (sess->url, &gs->url); AC_GST_ANAL(rc);              // Make the object from conn string
    rc = gst_rtsp_connection_create(gs->url, &gs->conn); AC_GST_ANAL(rc);        //Create connection object using url object
    rc = gst_rtsp_connection_connect (gs->conn, &conn_timeout); AC_GST_ANAL(rc); //Connect with timeout set

    gs->io_to.tv_sec = 60;
    gs->io_to.tv_usec = 0;
    if(gs->wowza_session = strdup(wowza_session_id), !gs->wowza_session) {
        pu_log(LL_ERROR, "%s: Memore allocation error at %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    sess->session = gs;
    sess->CSeq = 1;

    return 1;
on_error:
    pu_log(LL_DEBUG, "%s error section start", __FUNCTION__);
    clear_session(gs);
    sess->session = NULL;
    return 0;
}
/* 07 */
void ac_WowzaDown(t_at_rtsp_session* sess) {
    AT_DT_NR(sess->device, AC_WOWZA);
    clear_session(sess->session);
    sess->session = NULL;
}
/* 08 */
int ac_WowzaOptions(t_at_rtsp_session* sess) {
    AT_DT_RT(sess->device, AC_WOWZA, 0);
    GstRTSPResult rc;
    t_gst_session* gs = sess->session;
    GstRTSPMessage msg = {0};
    char num[10];
    snprintf(num, sizeof(num)-1, "%d", sess->CSeq);

    rc = gst_rtsp_message_init (&msg); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_init_request (&msg, GST_RTSP_OPTIONS, sess->url); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_CSEQ, num); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_USER_AGENT, AC_RTSP_CLIENT_NAME); AC_GST_ANAL(rc);

    rc = gst_rtsp_connection_send (gs->conn, &msg, &gs->io_to); AC_GST_ANAL(rc);    /* Send */

    gst_rtsp_message_unset(&msg);
    rc = gst_rtsp_connection_receive (gs->conn, &msg, &gs->io_to); AC_GST_ANAL(rc); /* Receive */
    if(msg.type != GST_RTSP_MESSAGE_RESPONSE) {
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, msg.type, GST_RTSP_MESSAGE_RESPONSE);
        goto on_error;
    }
    if(msg.type_data.response.code != GST_RTSP_STS_OK) {
        pu_log(LL_ERROR, "%s: bad answer: %s", __FUNCTION__, gst_rtsp_status_as_text(msg.type_data.response.code));
        goto on_error;
    }
//Now lets parse and show the answer
//1. Print headers!

    gchar* val;
    rc = gst_rtsp_message_get_header(&msg, GST_RTSP_HDR_CSEQ, &val, 0); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_get_header(&msg, GST_RTSP_HDR_SERVER, &val, 0); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_get_header(&msg, GST_RTSP_HDR_PUBLIC, &val, 0); AC_GST_ANAL(rc);

//Get body
    guint8 *data;
    guint size;

    rc = gst_rtsp_message_steal_body (&msg, &data, &size); AC_GST_ANAL(rc);

    if(size) {
        pu_log(LL_DEBUG, "%s: Body received = \n%s", __FUNCTION__, data);
    }

    gst_rtsp_message_unset(&msg);
    sess->CSeq++;

    return 1;

on_error:
    gst_rtsp_message_unset(&msg);
    return 0;
}
/* 09 */
int ac_WowzaAnnounce(t_at_rtsp_session* sess, const char* description) {
    AT_DT_RT(sess->device, AC_WOWZA, 0);
    GstRTSPResult rc;
    t_gst_session* gs = sess->session;
    GstRTSPMessage req = {0};
    GstRTSPMessage resp = {0};
    char num[10];

    contextId = 1009000;
    snprintf(num, sizeof(num)-1, "%d", sess->CSeq);
    contextId = 1005001;
    rc = gst_rtsp_message_init (&req); AC_GST_ANAL(rc);
    contextId = 1005002;
    rc = gst_rtsp_message_init_request (&req, GST_RTSP_ANNOUNCE, sess->url); AC_GST_ANAL(rc);
    contextId = 1005003;

    char new_description[1000] = {0};
    contextId = 1005004;
    if(!ac_rtsp_make_announce_body(new_description, sizeof(new_description), description, gs->url->host)) goto on_error;
    contextId = 1005005;
    pu_log(LL_DEBUG, "%s: New body for announnce =\n %s", __FUNCTION__, new_description);
    contextId = 1005006;
    if(!ac_rtsp_set_setup_urls(new_description, sess, AT_RTSP_CONCAT)) goto on_error;
    if(sess->audio_url) pu_log(LL_DEBUG, "%s: audio URL = %s", __FUNCTION__, sess->audio_url);
    if(sess->video_url) pu_log(LL_DEBUG, "%s: video URL = %s", __FUNCTION__, sess->video_url);
// Header
    rc = gst_rtsp_message_add_header (&req, GST_RTSP_HDR_CONTENT_TYPE, AC_RTSP_CONTENT_TYPE); AC_GST_ANAL(rc);
    contextId = 1005007;
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_CSEQ, num); AC_GST_ANAL(rc);
    contextId = 1005008;
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_USER_AGENT, AC_RTSP_CLIENT_NAME); AC_GST_ANAL(rc);
    contextId = 1005009;
    snprintf(num, sizeof(num)-1, "%lu", strlen(new_description));
    contextId = 1005010;
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_CONTENT_LENGTH, num); AC_GST_ANAL(rc);
    contextId = 1005011;
// Body
    rc = gst_rtsp_message_set_body(&req, (guint8 *)new_description, (guint)strlen(new_description));
    contextId = 1005012;

    rc = gst_rtsp_connection_send (gs->conn, &req, &gs->io_to); AC_GST_ANAL(rc);    /* Send */
    contextId = 1005013;

    rc = gst_rtsp_message_init (&resp); AC_GST_ANAL(rc);
    contextId = 1005014;
    rc = gst_rtsp_connection_receive (gs->conn, &resp, &gs->io_to); AC_GST_ANAL(rc); /* Receive */
    contextId = 1005015;

    if(resp.type != GST_RTSP_MESSAGE_RESPONSE) {
        contextId = 1005016;
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, resp.type, GST_RTSP_MESSAGE_RESPONSE);
        contextId = 1005017;
        goto on_error;
    }
    contextId = 1005018;
    if(resp.type_data.response.code == GST_RTSP_STS_OK) goto on_no_auth;    //Video restart w/o reconnection - same session ID
    contextId = 1005019;

    if(resp.type_data.response.code != GST_RTSP_STS_UNAUTHORIZED) {
        contextId = 1005020;
        pu_log(LL_ERROR, "%s: bad answer: %s", __FUNCTION__, gst_rtsp_status_as_text(resp.type_data.response.code));
        contextId = 1005021;
        goto on_error;
    }
//Auth step
    gchar* auth;
    gchar* session;
    contextId = 1005022;
    rc = gst_rtsp_message_get_header(&resp, GST_RTSP_HDR_WWW_AUTHENTICATE, &auth, 0); AC_GST_ANAL(rc);
    contextId = 1005023;
    rc = gst_rtsp_message_get_header(&resp, GST_RTSP_HDR_SESSION, &session, 0); AC_GST_ANAL(rc);
    contextId = 1005024;

    if(sess->rtsp_session_id = strdup(session), !sess->rtsp_session_id) {
        contextId = 1005025;
        pu_log(LL_ERROR, "%s: Memory allocation error", __FUNCTION__);
        contextId = 1005026;
        goto on_error;
    }

    GstRTSPAuthCredential** ac, **acc;
    contextId = 1005027;
    if (ac = gst_rtsp_message_parse_auth_credentials (&resp, GST_RTSP_HDR_WWW_AUTHENTICATE), !ac) {
        contextId = 1005028;
        pu_log(LL_ERROR, "%s error parsing WOWZA Auth Credentials", __FUNCTION__);
        contextId = 1005029;
        goto on_error;
    }

    acc = ac;
    contextId = 1005030;
    while (*acc) {
        contextId = 1005031;
        if ((*acc)->scheme != GST_RTSP_AUTH_DIGEST) {
            contextId = 1005032;
            pu_log(LL_ERROR, "%s: WOWZA proposed non-Digest auth. Not supported", __FUNCTION__);
            contextId = 1005033;
            gst_rtsp_auth_credentials_free (ac);
            contextId = 1005034;
            goto on_error;
        }
        GstRTSPAuthParam **param = (*acc)->params;
        contextId = 1005035;
        gst_rtsp_connection_clear_auth_params (gs->conn);
        contextId = 1005036;
        while (*param) {
            contextId = 1005037;
            gst_rtsp_connection_set_auth_param (gs->conn, (*param)->name, (*param)->value);
            contextId = 1005038;
                param++;
            contextId = 1005039;
        }
        contextId = 1005040;
        acc++;
        contextId = 1005041;
    }
    contextId = 1005042;
    gst_rtsp_auth_credentials_free (ac);
    contextId = 1005043;
    rc = gst_rtsp_connection_set_auth(gs->conn, GST_RTSP_AUTH_DIGEST, gs->wowza_session, gs->wowza_session); AC_GST_ANAL(rc);
    contextId = 1005044;
//Try to send it again
    rc = gst_rtsp_connection_send (gs->conn, &req, &gs->io_to); AC_GST_ANAL(rc);    /* Send */
    contextId = 1005045;

    rc = gst_rtsp_connection_receive (gs->conn, &resp, &gs->io_to); AC_GST_ANAL(rc); /* Receive */
    contextId = 1005046;

    if(resp.type != GST_RTSP_MESSAGE_RESPONSE) {
        contextId = 1005047;
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, resp.type, GST_RTSP_MESSAGE_RESPONSE);
        contextId = 1005048;
        goto on_error;
    }
    contextId = 1005049;
    if(resp.type_data.response.code != GST_RTSP_STS_OK) {
        contextId = 1005050;
        pu_log(LL_ERROR, "%s: bad answer: %s", gst_rtsp_status_as_text(resp.type_data.response.code));
        contextId = 1005051;
        goto on_error;
    }

on_no_auth:
    contextId = 1005052;
    sess->CSeq++;
    contextId = 1005053;

    gst_rtsp_message_unset(&req);
    contextId = 1005054;
    gst_rtsp_message_unset(&resp);
    contextId = 1005055;

    return 1;
on_error:
    contextId = 1005056;
    gst_rtsp_message_unset(&req);
    contextId = 1005057;
    gst_rtsp_message_unset(&resp);
    contextId = 1005058;
    return 0;
}
/* 10 */
int ac_WowzaSetup(t_at_rtsp_session* sess, int media_type) {
    AT_DT_RT(sess->device, AC_WOWZA, 0);
    GstRTSPResult rc;
    t_gst_session* gs = sess->session;
    GstRTSPMessage msg = {0};
    char num[10];
    snprintf(num, sizeof(num)-1, "%d", sess->CSeq);

    rc = gst_rtsp_message_init (&msg); AC_GST_ANAL(rc);
//1. Add trackId = 0 or 1 to the url;
    char tmp_url[AC_RTSP_MAX_URL_SIZE] = {0};


    if(media_type == AC_RTSP_VIDEO_SETUP)
        snprintf(tmp_url, sizeof(tmp_url), "%s/%s%s", sess->url, AC_TRACK, AC_VIDEO_TRACK);
    else //AUDIO
        snprintf(tmp_url, sizeof(tmp_url), "%s/%s%s", sess->url, AC_TRACK, AC_AUDIO_TRACK);

    rc = gst_rtsp_message_init_request (&msg, GST_RTSP_SETUP, tmp_url); AC_GST_ANAL(rc);

//2. Add transport header
    GstRTSPTransport *transport = NULL;
    rc = gst_rtsp_transport_new (&transport); AC_GST_ANAL(rc);

    if(ag_isCamInterleavedMode()) {
        transport->interleaved.min = (media_type == AC_RTSP_VIDEO_SETUP)?0:2;
        transport->interleaved.max = (media_type == AC_RTSP_VIDEO_SETUP)?1:3;
        transport->lower_transport = GST_RTSP_LOWER_TRANS_TCP;
    }
    else {
        if (media_type == AC_RTSP_VIDEO_SETUP) {
            transport->client_port.min = sess->media.rt_media.video.src.port.rtp;
            transport->client_port.max = sess->media.rt_media.video.src.port.rtcp;
        } else {
            transport->client_port.min = sess->media.rt_media.audio.src.port.rtp;
            transport->client_port.max = sess->media.rt_media.audio.src.port.rtcp;
        }
        transport->lower_transport = GST_RTSP_LOWER_TRANS_UDP;
    }
    transport->mode_play = FALSE;
    transport->mode_record = TRUE;
//    gs->transport->mode_record = TRUE;
    transport->profile = GST_RTSP_PROFILE_AVP;
    transport->trans = GST_RTSP_TRANS_RTP;

    gchar* text_transport = gst_rtsp_transport_as_text(transport);
    pu_log(LL_DEBUG, "%s: Transport string for %s prepared = %s", __FUNCTION__, (media_type == AC_RTSP_VIDEO_SETUP)?"video":"audio", text_transport);
    rc = gst_rtsp_message_add_header (&msg, GST_RTSP_HDR_TRANSPORT, text_transport); AC_GST_ANAL(rc);
    g_free(text_transport); text_transport = NULL;
    gst_rtsp_transport_free(transport); transport = NULL;

    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_CSEQ, num); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_USER_AGENT, AC_RTSP_CLIENT_NAME); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_SESSION, sess->rtsp_session_id); AC_GST_ANAL(rc);

//We'll see what happens with the session & AUTH

    rc = gst_rtsp_connection_send (gs->conn, &msg, &gs->io_to); AC_GST_ANAL(rc);    /* Send */
    gst_rtsp_message_unset(&msg);

    rc = gst_rtsp_connection_receive (gs->conn, &msg, &gs->io_to); AC_GST_ANAL(rc); /* Receive */
    if(msg.type != GST_RTSP_MESSAGE_RESPONSE) {
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, msg.type, GST_RTSP_MESSAGE_RESPONSE);
        goto on_error;
    }
    if(msg.type_data.response.code != GST_RTSP_STS_OK) {
        pu_log(LL_ERROR, "%s: bad answer: %s", gst_rtsp_status_as_text(msg.type_data.response.code));
        goto on_error;
    }

    rc = gst_rtsp_message_get_header(&msg, GST_RTSP_HDR_TRANSPORT, &text_transport, 0); AC_GST_ANAL(rc);
    pu_log(LL_DEBUG, "%s: Transport string for %s received = %s", __FUNCTION__, (media_type == AC_RTSP_VIDEO_SETUP)?"video":"audio", text_transport);

    rc = gst_rtsp_transport_new (&transport); AC_GST_ANAL(rc);
    rc = gst_rtsp_transport_parse(text_transport, transport); AC_GST_ANAL(rc);
//Save server port
    if(!ag_isCamInterleavedMode()) {
        if (media_type == AC_RTSP_VIDEO_SETUP) {
            sess->media.rt_media.video.dst.port.rtp = transport->server_port.min;
            sess->media.rt_media.video.dst.port.rtcp = transport->server_port.max;
            if (sess->media.rt_media.video.dst.ip = strdup(transport->source), !sess->media.rt_media.video.dst.ip) {
                pu_log(LL_ERROR, "%s: Memory allocation error on %d", __FUNCTION__, __LINE__);
                goto on_error;
            }
        }
        else {
            sess->media.rt_media.audio.dst.port.rtp = transport->server_port.min;
            sess->media.rt_media.audio.dst.port.rtcp = transport->server_port.max;
            if (sess->media.rt_media.audio.dst.ip = strdup(transport->source), !sess->media.rt_media.audio.dst.ip) {
                pu_log(LL_ERROR, "%s: Memory allocation error on %d", __FUNCTION__, __LINE__);
                goto on_error;
            }
        }
    }
    sess->CSeq++;

    gst_rtsp_transport_free(transport);
    gst_rtsp_message_unset(&msg);
    return 1;
on_error:
    if(transport) gst_rtsp_transport_free(transport);
    gst_rtsp_message_unset(&msg);
    return 0;
}
/* 11 */
int ac_WowzaPlay(t_at_rtsp_session* sess) {
    AT_DT_RT(sess->device, AC_WOWZA, 0);
    GstRTSPResult rc;
    t_gst_session* gs = sess->session;
    GstRTSPMessage msg = {0};
    char num[10];
    snprintf(num, sizeof(num)-1, "%d", sess->CSeq);

    rc = gst_rtsp_message_init (&msg); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_init_request(&msg, GST_RTSP_RECORD, sess->url); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_RANGE, AC_PLAY_RANGE); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_CSEQ, num); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_USER_AGENT, AC_RTSP_CLIENT_NAME); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_SESSION, sess->rtsp_session_id); AC_GST_ANAL(rc);

    rc = gst_rtsp_connection_send (gs->conn, &msg, &gs->io_to); AC_GST_ANAL(rc);    /* Send */
    gst_rtsp_message_unset(&msg);

    rc = gst_rtsp_connection_receive (gs->conn, &msg, &gs->io_to); AC_GST_ANAL(rc); /* Receive */
    if(msg.type != GST_RTSP_MESSAGE_RESPONSE) {
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, msg.type, GST_RTSP_MESSAGE_RESPONSE);
        goto on_error;
    }
    if(msg.type_data.response.code != GST_RTSP_STS_OK) {
        pu_log(LL_ERROR, "%s: bad answer: %s", __FUNCTION__, gst_rtsp_status_as_text(msg.type_data.response.code));
        goto on_error;
    }

    gst_rtsp_message_unset(&msg);
    return 1;
on_error:
    gst_rtsp_message_unset(&msg);
    return 0;
}
/* 12 */
int ac_WowzaTeardown(t_at_rtsp_session* sess) {
    AT_DT_RT(sess->device, AC_WOWZA, 0);
    GstRTSPResult rc;
    t_gst_session* gs = sess->session;
    GstRTSPMessage msg = {0};
    char num[10];
    snprintf(num, sizeof(num)-1, "%d", sess->CSeq);

    rc = gst_rtsp_message_init (&msg); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_init_request (&msg, GST_RTSP_TEARDOWN, sess->url); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_CSEQ, num); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_USER_AGENT, AC_RTSP_CLIENT_NAME); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&msg, GST_RTSP_HDR_SESSION, sess->rtsp_session_id); AC_GST_ANAL(rc);

    rc = gst_rtsp_connection_send (gs->conn, &msg, &gs->io_to); AC_GST_ANAL(rc);    /* Send */

    gst_rtsp_message_unset(&msg);
    rc = gst_rtsp_connection_receive (gs->conn, &msg, &gs->io_to); AC_GST_ANAL(rc); /* Receive */
    if(msg.type != GST_RTSP_MESSAGE_RESPONSE) {
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, msg.type, GST_RTSP_MESSAGE_RESPONSE);
        goto on_error;
    }
    if(msg.type_data.response.code != GST_RTSP_STS_OK) {
        pu_log(LL_ERROR, "%s: bad answer: %s", __FUNCTION__, gst_rtsp_status_as_text(msg.type_data.response.code));
        goto on_error;
    }

    gst_rtsp_message_unset(&msg);
    sess->CSeq++;

    return 1;

on_error:
    gst_rtsp_message_unset(&msg);
    return 0;
}
/* 13 */
const char* ac_make_wowza_url(char *url, size_t size, const char* protocol, const char* vs_url, int port, const char* vs_session_id) {
    char s_port[20];
    url[0] = '\0';
    sprintf(s_port, "%d", port);

    if((strlen(vs_url)+strlen(s_port)+strlen(vs_session_id)+strlen(DEFAULT_PPC_VIDEO_FOLDER) + 4) > (size-1)) {
        pu_log(LL_ERROR, "%s: buffer size %d too low. VS URL can't be constructed", __FUNCTION__, size);
        pu_log(LL_ERROR, "%s: vs_url=%s, s_port = %s, vs_session_id=%s, DEFAULT_PPC_VIDEO_FOLDER=%s",
            __FUNCTION__, vs_url, s_port, vs_session_id, DEFAULT_PPC_VIDEO_FOLDER);
        return url;
    }
    sprintf(url, "%s://%s:%s/%s/%s", protocol, vs_url, s_port, DEFAULT_PPC_VIDEO_FOLDER, vs_session_id);
    return url;

}
/* 14 */
int getWowzaConnSocket(t_at_rtsp_session* sess) {
    AT_DT_RT(sess->device, AC_WOWZA, -1);

    t_gst_session* gs = sess->session;
    GSocket* gsock = gst_rtsp_connection_get_write_socket(gs->conn);
    if(!gsock) {
        pu_log(LL_ERROR, "%s: the gst_rtsp_connection_get_write_socket() return NULL", __FUNCTION__);
        return -1;
    }
    return g_socket_get_fd(gsock);
}