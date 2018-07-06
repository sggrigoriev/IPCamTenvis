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

static int make_ip_from_host(char* ip, size_t size, const char* host) {

    struct hostent* hn = gethostbyname(host);
    if(!hn) {
        pu_log(LL_ERROR, "%s: Can't get IP of VS server: %d - %s", __FUNCTION__, h_errno, strerror(h_errno));
        return 0;
    }
    struct sockaddr_in s;
    memcpy(&s.sin_addr, hn->h_addr_list[0], (size_t)hn->h_length);
    strncpy(ip, inet_ntoa(s.sin_addr), size);

    return 1;
}

/**
 * gst_sdp_media_copy_replace: - copypizded from https://github.com/jojva/gst-plugins-base/blob/master/gst-libs/gst/sdp/gstsdpmessage.c
 * gst_sdp_media_copy.
 *
 * Makes copy and replaces some media attributes: Finds control attribute for video&audio medias and replaca it on controls at parameters
 * @media: a #GstSDPMedia
 * @copy: (out) (transfer full): pointer to new #GstSDPMedia
 * control: replacement value for "a=contrlol:" in media
 *
 * Allocate a new copy of @media and store the result in @copy. The value in
 * @copy should be release with gst_sdp_media_free function.
 *
 * Returns: a #GstSDPResult
 *
 * Since: 1.2
 */
#define FREE_STRING(field)              g_free (field); (field) = NULL
#define REPLACE_STRING(field, val)      FREE_STRING(field); (field) = g_strdup (val)

GstSDPResult
gst_sdp_media_copy_n_replace (const GstSDPMedia * media, GstSDPMedia ** copy, const char* control)
{
    GstSDPResult ret;
    GstSDPMedia *cp;
    guint i, len;

    if (media == NULL)
        return GST_SDP_EINVAL;

    ret = gst_sdp_media_new (copy);
    if (ret != GST_SDP_OK)
        return ret;

    cp = *copy;

    REPLACE_STRING (cp->media, media->media);
    cp->port = media->port;
    cp->num_ports = media->num_ports;
    REPLACE_STRING (cp->proto, media->proto);

    len = gst_sdp_media_formats_len (media);
    for (i = 0; i < len; i++) {
        gst_sdp_media_add_format (cp, gst_sdp_media_get_format (media, i));
    }

    REPLACE_STRING (cp->information, media->information);

    len = gst_sdp_media_connections_len (media);
    for (i = 0; i < len; i++) {
        const GstSDPConnection *connection =
                gst_sdp_media_get_connection (media, i);
        gst_sdp_media_add_connection (cp, connection->nettype, connection->addrtype,
                                      connection->address, connection->ttl, connection->addr_number);
    }

    len = gst_sdp_media_bandwidths_len (media);
    for (i = 0; i < len; i++) {
        const GstSDPBandwidth *bw = gst_sdp_media_get_bandwidth (media, i);
        gst_sdp_media_add_bandwidth (cp, bw->bwtype, bw->bandwidth);
    }

    gst_sdp_media_set_key (cp, media->key.type, media->key.data);

    len = gst_sdp_media_attributes_len (media);
    for (i = 0; i < len; i++) {
        const GstSDPAttribute *att = gst_sdp_media_get_attribute (media, i);
        if(!strcmp(att->key, "control")) {
            GstSDPAttribute* m_attr;
            if(m_attr = calloc(1, sizeof(GstSDPAttribute)), !m_attr) {
                pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
                gst_sdp_media_free(cp);
                return GST_SDP_EINVAL;
            }
            REPLACE_STRING (m_attr->key, "control");
            REPLACE_STRING(m_attr->value, control);
            gst_sdp_media_add_attribute (cp, m_attr->key, m_attr->value);
        }
        else {
            gst_sdp_media_add_attribute(cp, att->key, att->value);
        }
    }

    return GST_SDP_OK;
}

/***************************************************
 * Get attribute value ("a=") from sdp with attr_name, given media type (video or audoi)
 * if media_type is NULL then the common attribute takes.
 * @param sdp           txt RTSP SDP presentation
 * @param attr_name     attribute name (a=<attr_name>)
 * @param media_type    NULL, video or audio
 * @return              NULL if attr not found or attribute value as null-terminated string
 */
const char* ac_wowzaGetAttr(const char* sdp_ascii, const char* attr_name, const char* media_type) {
    GstSDPMessage *sdp;
    const char* ret = NULL;

    if(gst_sdp_message_new (&sdp) != GST_SDP_OK) {
        pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
        return ret;
    }
    if(gst_sdp_message_parse_buffer ((const guint8*)sdp_ascii, (guint)strlen(sdp_ascii), sdp) != GST_SDP_OK) {
        pu_log(LL_ERROR, "%s: Error parsing sdp ASCII description", __FUNCTION__);
        gst_sdp_message_free(sdp);
        return ret;
    }
    if(!media_type) {   /* looking for connon attribute */
        ret = gst_sdp_message_get_attribute_val(sdp, attr_name);
    }
    else {              /* looking for the attribute of specific media type */
        guint i = 0;
        for (i = 0; i < gst_sdp_message_medias_len(sdp); i++) {
            const GstSDPMedia *sdp_media = gst_sdp_message_get_media(sdp, i);
            if((sdp_media->media != NULL) && !strcmp(sdp_media->media, media_type)) {
                ret = gst_sdp_media_get_attribute_val(sdp_media, attr_name);
                break;
            }
        }
    }
    return ret;
}

/********************************************
 * Finds control attribute for video&audio medias and replaca it on controls at parameters
 * @param sdp
 * @param video_control
 * @param audio_control
 * @return
 */
int set_media_controls(GstSDPMessage* sdp, const char* video_control, const char* audio_control) {
    GstSDPMedia **new_sdp_media;
    guint old_media_len = gst_sdp_message_medias_len(sdp);
    if(new_sdp_media = calloc(old_media_len, sizeof(GstSDPMedia*)), !(new_sdp_media)) {
        pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
        return 0;
    }
    guint i;
    for(i = 0; i < old_media_len; i++) {
        const GstSDPMedia* sdp_media = gst_sdp_message_get_media(sdp, i);
        if(!strcmp(sdp_media->media, "video")) {
            if(gst_sdp_media_copy_n_replace(sdp_media, new_sdp_media+i, video_control) != GST_SDP_OK) {
                pu_log(LL_ERROR, "%s: Error video control attr replacement", __FUNCTION__);
                free(new_sdp_media);
                return 0;
            }
        }
        if(!strcmp(sdp_media->media, "audio")) {
            if(gst_sdp_media_copy_n_replace(sdp_media, new_sdp_media+i, audio_control) != GST_SDP_OK) {
                pu_log(LL_ERROR, "%s: Error audio control attr replacement", __FUNCTION__);
                free(new_sdp_media);
                return 0;
            }
        }
    }

    g_array_free(sdp->medias, TRUE);
    for(i = 0; i < old_media_len; i++) {
        gst_sdp_message_add_media(sdp, new_sdp_media[i]);
    }
    free(new_sdp_media); /* NB! The array is free but elements are not - they are kept in SDP */
    return 1;
}

/* Replace
 * o=StreamingServer 3331435948 1116907222000 IN IP4 10.42.0.115 -> o=- 0 0 IN IP4 127.0.0.1
 * c=IN IP4 0.0.0.0 -> c=IN IP4 <WOWZA IP>
 * set a=control:*
 * And set media contrlols for video and audio (if any) for trackID=0 and tractID=1
 * Return NULL atring if error
*/
#define MAD_ERR() {pu_log(LL_ERROR, "%s: GST error at %d", __FUNCTION__, __LINE__); goto on_error;}
static char* make_announce_body(char* buf, size_t size, const char* cam_descr, const char* host) {
    GstSDPMessage * sdp = NULL;

    if(gst_sdp_message_new (&sdp) != GST_SDP_OK) MAD_ERR();
    if(gst_sdp_message_parse_buffer ((const guint8*)cam_descr, (guint)strlen(cam_descr), sdp) != GST_SDP_OK) MAD_ERR();

/* Set o= */
    if(gst_sdp_message_set_origin(sdp,
                                    "-",
                                    "0",
                                    "0",
                                    "IN",
                                    "IP4",
                                    "127.0.0.1"
                                    ) !=  GST_SDP_OK) MAD_ERR();
/* c= ... -> c = IN IP4 host */
    if(gst_sdp_message_set_connection(sdp, "IN", "IP4", host, 0, 0) != GST_SDP_OK) MAD_ERR();

/* add a=control:* */
    if(gst_sdp_message_add_attribute(sdp, "control", "*") != GST_SDP_OK) MAD_ERR();

/* And set media contrlols for video and audio (if any) for trackID=0 and tractID=1 */
    if(!set_media_controls(sdp, "trackID=0", "trackID=1")) goto on_error;

    char* txt = gst_sdp_message_as_text(sdp);
    pu_log(LL_DEBUG, "%s: SDP message for ANNOUCE:\n%s", __FUNCTION__, txt);
    strncpy(buf, txt, size-1);
    free(txt);
    gst_sdp_message_free(sdp);
    return buf;

on_error:
    if(sdp)gst_sdp_message_free(sdp);
    return NULL;
}

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
void ac_WowzaDown(t_at_rtsp_session* sess) {
    AT_DT_NR(sess->device, AC_WOWZA);
    clear_session(sess->session);
    sess->session = NULL;
}

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
        pu_log(LL_ERROR, "%s: bad answer: %s", gst_rtsp_status_as_text(msg.type_data.response.code));
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
        pu_log(LL_DEBUG, "%s: Body received = \n%s", data);
    }

    gst_rtsp_message_unset(&msg);
    sess->CSeq++;

    return 1;

on_error:
    gst_rtsp_message_unset(&msg);
    return 0;
}
int ac_WowzaAnnounce(t_at_rtsp_session* sess, const char* description) {
    AT_DT_RT(sess->device, AC_WOWZA, 0);
    GstRTSPResult rc;
    t_gst_session* gs = sess->session;
    GstRTSPMessage req = {0};
    GstRTSPMessage resp = {0};
    char num[10];
    snprintf(num, sizeof(num)-1, "%d", sess->CSeq);

    char new_description[1000] = {0};
    if(!make_announce_body(new_description, sizeof(new_description), description, gs->url->host)) goto on_error;

    rc = gst_rtsp_message_init (&req); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_init_request (&req, GST_RTSP_ANNOUNCE, sess->url); AC_GST_ANAL(rc);
// Header
    rc = gst_rtsp_message_add_header (&req, GST_RTSP_HDR_CONTENT_TYPE, AC_RTSP_CONTENT_TYPE); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_CSEQ, num); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_USER_AGENT, AC_RTSP_CLIENT_NAME); AC_GST_ANAL(rc);
    snprintf(num, sizeof(num)-1, "%lu", strlen(new_description));
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_CONTENT_LENGTH, num); AC_GST_ANAL(rc);
// Body
    rc = gst_rtsp_message_set_body(&req, (guint8 *)new_description, (guint)strlen(new_description));

    rc = gst_rtsp_connection_send (gs->conn, &req, &gs->io_to); AC_GST_ANAL(rc);    /* Send */

    rc = gst_rtsp_message_init (&resp); AC_GST_ANAL(rc);
    rc = gst_rtsp_connection_receive (gs->conn, &resp, &gs->io_to); AC_GST_ANAL(rc); /* Receive */

    if(resp.type != GST_RTSP_MESSAGE_RESPONSE) {
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, resp.type, GST_RTSP_MESSAGE_RESPONSE);
        goto on_error;
    }
    if(resp.type_data.response.code == GST_RTSP_STS_OK) goto on_no_auth;    //Video restart w/o reconnection - same session ID

    if(resp.type_data.response.code != GST_RTSP_STS_UNAUTHORIZED) {
        pu_log(LL_ERROR, "%s: bad answer: %s", __FUNCTION__, gst_rtsp_status_as_text(resp.type_data.response.code));
        goto on_error;
    }
//Auth step
    gchar* auth;
    gchar* session;
    rc = gst_rtsp_message_get_header(&resp, GST_RTSP_HDR_WWW_AUTHENTICATE, &auth, 0); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_get_header(&resp, GST_RTSP_HDR_SESSION, &session, 0); AC_GST_ANAL(rc);

    if(sess->rtsp_session_id = strdup(session), !sess->rtsp_session_id) {
        pu_log(LL_ERROR, "%s: Memory allocation error", __FUNCTION__);
        goto on_error;
    }

    GstRTSPAuthCredential** ac, **acc;
    if (ac = gst_rtsp_message_parse_auth_credentials (&resp, GST_RTSP_HDR_WWW_AUTHENTICATE), !ac) {
        pu_log(LL_ERROR, "%s error parsing WOWZA Auth Credentials", __FUNCTION__);
        goto on_error;
    }

    acc = ac;
    while (*acc) {
        if ((*acc)->scheme != GST_RTSP_AUTH_DIGEST) {
            pu_log(LL_ERROR, "%s: WOWZA proposed non-Digest auth. Not supported", __FUNCTION__);
            gst_rtsp_auth_credentials_free (ac);
            goto on_error;
        }
        GstRTSPAuthParam **param = (*acc)->params;
        gst_rtsp_connection_clear_auth_params (gs->conn);
        while (*param) {
            gst_rtsp_connection_set_auth_param (gs->conn, (*param)->name, (*param)->value);
                param++;
        }
        acc++;
    }
    gst_rtsp_auth_credentials_free (ac);
    rc = gst_rtsp_connection_set_auth(gs->conn, GST_RTSP_AUTH_DIGEST, gs->wowza_session, gs->wowza_session); AC_GST_ANAL(rc);
//Try to send it again
    rc = gst_rtsp_connection_send (gs->conn, &req, &gs->io_to); AC_GST_ANAL(rc);    /* Send */

    rc = gst_rtsp_connection_receive (gs->conn, &resp, &gs->io_to); AC_GST_ANAL(rc); /* Receive */

    if(resp.type != GST_RTSP_MESSAGE_RESPONSE) {
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, resp.type, GST_RTSP_MESSAGE_RESPONSE);
        goto on_error;
    }
    if(resp.type_data.response.code != GST_RTSP_STS_OK) {
        pu_log(LL_ERROR, "%s: bad answer: %s", gst_rtsp_status_as_text(resp.type_data.response.code));
        goto on_error;
    }

on_no_auth:
    sess->CSeq++;

    gst_rtsp_message_unset(&req);
    gst_rtsp_message_unset(&resp);

    return 1;
on_error:
    gst_rtsp_message_unset(&req);
    gst_rtsp_message_unset(&resp);
    return 0;
}
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
        pu_log(LL_ERROR, "%s: bad answer: %s", gst_rtsp_status_as_text(msg.type_data.response.code));
        goto on_error;
    }

    gst_rtsp_message_unset(&msg);
    sess->CSeq++;

    return 1;

on_error:
    gst_rtsp_message_unset(&msg);
    return 0;
}

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

int getWowzaConnSocket(t_at_rtsp_session* sess) {
    AT_DT_RT(sess->device, AC_WOWZA, -1);

    t_gst_session* gs = sess->session;
    GSocket* gsock = gst_rtsp_connection_get_write_socket(gs->conn);
    if(!gsock) {
        pu_log(LL_ERROR, "getWowzaConnSocket: the gst_rtsp_connection_get_write_socket() return NULL");
        return -1;
    }
    return g_socket_get_fd(gsock);
}