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
/* 01 */
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

/* Dedined below */
static int set_media_controls(GstSDPMessage* sdp, const char* video_control, const char* audio_control);
/* Replace
 * o=StreamingServer 3331435948 1116907222000 IN IP4 10.42.0.115 -> o=- 0 0 IN IP4 127.0.0.1
 * c=IN IP4 0.0.0.0 -> c=IN IP4 <WOWZA IP>
 * set a=control:*
 * And set media contrlols for video and audio (if any) for trackID=0 and tractID=1
 * Return NULL atring if error
*/
#define MAD_ERR() {pu_log(LL_ERROR, "%s: GST error at %d", __FUNCTION__, __LINE__); goto on_error;}
/* 02 */
static char* make_announce_body(char* buf, size_t size, const char* cam_descr, const char* host) {
    GstSDPMessage * sdp = NULL;
    contextId = 1002000;
    if(gst_sdp_message_new (&sdp) != GST_SDP_OK) MAD_ERR();
    contextId = 1002001;
    if(gst_sdp_message_parse_buffer ((const guint8*)cam_descr, (guint)strlen(cam_descr), sdp) != GST_SDP_OK) MAD_ERR();
    contextId = 1002002;

/* Set o= */
    if(gst_sdp_message_set_origin(sdp,
                                  "-",
                                  "0",
                                  "0",
                                  "IN",
                                  "IP4",
                                  "127.0.0.1"
    ) !=  GST_SDP_OK) MAD_ERR();
    contextId = 1002003;
/* c= ... -> c = IN IP4 host */
    if(gst_sdp_message_set_connection(sdp, "IN", "IP4", host, 0, 0) != GST_SDP_OK) MAD_ERR();
    contextId = 1002004;
/* add a=control:* */
    if(gst_sdp_message_add_attribute(sdp, "control", "*") != GST_SDP_OK) MAD_ERR();
    contextId = 1002005;

/* And set media contrlols for video and audio (if any) for trackID=0 and tractID=1 */
    if(!set_media_controls(sdp, "trackID=0", "trackID=1")) goto on_error;
    contextId = 1002006;
    char* txt = gst_sdp_message_as_text(sdp);
    contextId = 1002007;
    pu_log(LL_DEBUG, "%s: SDP message for ANNOUCE:\n%s", __FUNCTION__, txt);
    contextId = 1002008;
    strncpy(buf, txt, size-1);
    contextId = 1002009;
    free(txt);
    contextId = 1002010;
    gst_sdp_message_free(sdp);
    contextId = 1002011;
    return buf;

on_error:
    if(sdp)gst_sdp_message_free(sdp);
    contextId = 1002012;
    return NULL;
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
/* 03 */
GstSDPResult
gst_sdp_media_copy_n_replace (const GstSDPMedia * media, GstSDPMedia ** copy, const char* control)
{
    GstSDPResult ret;
    GstSDPMedia *cp;
    guint i, len;
    contextId = 1003000;
    if (media == NULL)
        return GST_SDP_EINVAL;
    contextId = 1003001;

    ret = gst_sdp_media_new (copy);
    contextId = 1003002;
    if (ret != GST_SDP_OK)
        return ret;
    contextId = 1003003;
    cp = *copy;
    contextId = 1003004;
    REPLACE_STRING (cp->media, media->media);
    contextId = 1003005;
    cp->port = media->port;
    contextId = 1003006;
    cp->num_ports = media->num_ports;
    contextId = 1003007;
    REPLACE_STRING (cp->proto, media->proto);
    contextId = 1003008;

    len = gst_sdp_media_formats_len (media);
    contextId = 1003009;
    for (i = 0; i < len; i++) {
        contextId = 1003010;
        gst_sdp_media_add_format (cp, gst_sdp_media_get_format (media, i));
        contextId = 1003011;
    }
    contextId = 1003012;
    REPLACE_STRING (cp->information, media->information);
    contextId = 1003013;

    len = gst_sdp_media_connections_len (media);
    contextId = 1003014;
    for (i = 0; i < len; i++) {
        contextId = 1003015;
        const GstSDPConnection *connection =
                gst_sdp_media_get_connection (media, i);
        contextId = 1003016;
        gst_sdp_media_add_connection (cp, connection->nettype, connection->addrtype,
                                      connection->address, connection->ttl, connection->addr_number);
        contextId = 1003017;
    }
    contextId = 1003018;
    len = gst_sdp_media_bandwidths_len (media);
    contextId = 1003019;
    for (i = 0; i < len; i++) {
        contextId = 1003020;
        const GstSDPBandwidth *bw = gst_sdp_media_get_bandwidth (media, i);
        contextId = 1003021;
        gst_sdp_media_add_bandwidth (cp, bw->bwtype, bw->bandwidth);
        contextId = 1003022;
    }
    contextId = 1003023;
    gst_sdp_media_set_key (cp, media->key.type, media->key.data);
    contextId = 1003024;
    len = gst_sdp_media_attributes_len (media);
    contextId = 1003025;
    for (i = 0; i < len; i++) {
        contextId = 1003026;
        const GstSDPAttribute *att = gst_sdp_media_get_attribute (media, i);
        contextId = 1003027;
        if(!strcmp(att->key, "control")) {
            contextId = 1003028;
            GstSDPAttribute* m_attr;
            if(m_attr = calloc(1, sizeof(GstSDPAttribute)), !m_attr) {
                contextId = 1003029;
                pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
                contextId = 1003030;
                gst_sdp_media_free(cp);
                contextId = 1003031;
                return GST_SDP_EINVAL;
            }
            contextId = 1003032;
            REPLACE_STRING (m_attr->key, "control");
            contextId = 1003033;
            REPLACE_STRING(m_attr->value, control);
            contextId = 1003034;
            gst_sdp_media_add_attribute (cp, m_attr->key, m_attr->value);
            contextId = 1003035;
        }
        else {
            contextId = 1003036;
            gst_sdp_media_add_attribute(cp, att->key, att->value);
            contextId = 1003037;
        }
    }
    contextId = 1003038;

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
 /* 04 */
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
/* 05 */
static int set_media_controls(GstSDPMessage* sdp, const char* video_control, const char* audio_control) {
    GstSDPMedia **new_sdp_media;
    contextId = 1005000;
    guint old_media_len = gst_sdp_message_medias_len(sdp);
    contextId = 1005001;
    if(new_sdp_media = calloc(old_media_len, sizeof(GstSDPMedia*)), !(new_sdp_media)) {
        contextId = 1005002;
        pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
        contextId = 1005003;
        return 0;
    }
    guint i;
    contextId = 1005004;
    for(i = 0; i < old_media_len; i++) {
        contextId = 1005005;
        const GstSDPMedia* sdp_media = gst_sdp_message_get_media(sdp, i);
        contextId = 1005006;
        if(!strcmp(sdp_media->media, "video")) {
            contextId = 1005007;
            if(gst_sdp_media_copy_n_replace(sdp_media, new_sdp_media+i, video_control) != GST_SDP_OK) {
                contextId = 1005008;
                pu_log(LL_ERROR, "%s: Error video control attr replacement", __FUNCTION__);
                contextId = 1005009;
                free(new_sdp_media);
                contextId = 1005010;
                return 0;
            }
        }
        if(!strcmp(sdp_media->media, "audio")) {
            contextId = 1005011;
            if(gst_sdp_media_copy_n_replace(sdp_media, new_sdp_media+i, audio_control) != GST_SDP_OK) {
                contextId = 1005012;
                pu_log(LL_ERROR, "%s: Error audio control attr replacement", __FUNCTION__);
                contextId = 1005013;
                free(new_sdp_media);
                contextId = 1005014;
                return 0;
            }
        }
    }

    g_array_free(sdp->medias, TRUE);
    contextId = 1005015;
    for(i = 0; i < old_media_len; i++) {
        contextId = 1005016;
        gst_sdp_message_add_media(sdp, new_sdp_media[i]);
    }
    contextId = 1005017;
    free(new_sdp_media); /* NB! The array is free but elements are not - they are kept in SDP */
    contextId = 1005018;
    return 1;
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
    if(!make_announce_body(new_description, sizeof(new_description), description, gs->url->host)) goto on_error;
    contextId = 1005005;

// Header
    rc = gst_rtsp_message_add_header (&req, GST_RTSP_HDR_CONTENT_TYPE, AC_RTSP_CONTENT_TYPE); AC_GST_ANAL(rc);
    contextId = 1005006;
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_CSEQ, num); AC_GST_ANAL(rc);
    contextId = 1005007;
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_USER_AGENT, AC_RTSP_CLIENT_NAME); AC_GST_ANAL(rc);
    contextId = 1005008;
    snprintf(num, sizeof(num)-1, "%lu", strlen(new_description));
    contextId = 1005009;
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_CONTENT_LENGTH, num); AC_GST_ANAL(rc);
    contextId = 1005009;
// Body
    rc = gst_rtsp_message_set_body(&req, (guint8 *)new_description, (guint)strlen(new_description));
    contextId = 1005010;

    rc = gst_rtsp_connection_send (gs->conn, &req, &gs->io_to); AC_GST_ANAL(rc);    /* Send */
    contextId = 1005011;

    rc = gst_rtsp_message_init (&resp); AC_GST_ANAL(rc);
    contextId = 1005012;
    rc = gst_rtsp_connection_receive (gs->conn, &resp, &gs->io_to); AC_GST_ANAL(rc); /* Receive */
    contextId = 1005013;

    if(resp.type != GST_RTSP_MESSAGE_RESPONSE) {
        contextId = 1005014;
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, resp.type, GST_RTSP_MESSAGE_RESPONSE);
        contextId = 1005015;
        goto on_error;
    }
    contextId = 1005016;
    if(resp.type_data.response.code == GST_RTSP_STS_OK) goto on_no_auth;    //Video restart w/o reconnection - same session ID
    contextId = 1005017;

    if(resp.type_data.response.code != GST_RTSP_STS_UNAUTHORIZED) {
        contextId = 1005018;
        pu_log(LL_ERROR, "%s: bad answer: %s", __FUNCTION__, gst_rtsp_status_as_text(resp.type_data.response.code));
        contextId = 1005019;
        goto on_error;
    }
//Auth step
    gchar* auth;
    gchar* session;
    contextId = 1005020;
    rc = gst_rtsp_message_get_header(&resp, GST_RTSP_HDR_WWW_AUTHENTICATE, &auth, 0); AC_GST_ANAL(rc);
    contextId = 1005021;
    rc = gst_rtsp_message_get_header(&resp, GST_RTSP_HDR_SESSION, &session, 0); AC_GST_ANAL(rc);
    contextId = 1005022;

    if(sess->rtsp_session_id = strdup(session), !sess->rtsp_session_id) {
        contextId = 1005023;
        pu_log(LL_ERROR, "%s: Memory allocation error", __FUNCTION__);
        contextId = 1005024;
        goto on_error;
    }

    GstRTSPAuthCredential** ac, **acc;
    contextId = 1005025;
    if (ac = gst_rtsp_message_parse_auth_credentials (&resp, GST_RTSP_HDR_WWW_AUTHENTICATE), !ac) {
        contextId = 1005026;
        pu_log(LL_ERROR, "%s error parsing WOWZA Auth Credentials", __FUNCTION__);
        contextId = 1005027;
        goto on_error;
    }

    acc = ac;
    contextId = 1005028;
    while (*acc) {
        contextId = 1005029;
        if ((*acc)->scheme != GST_RTSP_AUTH_DIGEST) {
            contextId = 1005030;
            pu_log(LL_ERROR, "%s: WOWZA proposed non-Digest auth. Not supported", __FUNCTION__);
            contextId = 1005031;
            gst_rtsp_auth_credentials_free (ac);
            contextId = 1005032;
            goto on_error;
        }
        GstRTSPAuthParam **param = (*acc)->params;
        contextId = 1005033;
        gst_rtsp_connection_clear_auth_params (gs->conn);
        contextId = 1005034;
        while (*param) {
            contextId = 1005035;
            gst_rtsp_connection_set_auth_param (gs->conn, (*param)->name, (*param)->value);
            contextId = 1005036;
                param++;
            contextId = 1005037;
        }
        contextId = 1005038;
        acc++;
        contextId = 1005039;
    }
    contextId = 1005040;
    gst_rtsp_auth_credentials_free (ac);
    contextId = 1005041;
    rc = gst_rtsp_connection_set_auth(gs->conn, GST_RTSP_AUTH_DIGEST, gs->wowza_session, gs->wowza_session); AC_GST_ANAL(rc);
    contextId = 1005042;
//Try to send it again
    rc = gst_rtsp_connection_send (gs->conn, &req, &gs->io_to); AC_GST_ANAL(rc);    /* Send */
    contextId = 1005043;

    rc = gst_rtsp_connection_receive (gs->conn, &resp, &gs->io_to); AC_GST_ANAL(rc); /* Receive */
    contextId = 1005044;

    if(resp.type != GST_RTSP_MESSAGE_RESPONSE) {
        contextId = 1005045;
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, resp.type, GST_RTSP_MESSAGE_RESPONSE);
        contextId = 1005046;
        goto on_error;
    }
    contextId = 1005047;
    if(resp.type_data.response.code != GST_RTSP_STS_OK) {
        contextId = 1005048;
        pu_log(LL_ERROR, "%s: bad answer: %s", gst_rtsp_status_as_text(resp.type_data.response.code));
        contextId = 1005049;
        goto on_error;
    }

on_no_auth:
    contextId = 1005050;
    sess->CSeq++;
    contextId = 1005051;

    gst_rtsp_message_unset(&req);
    contextId = 1005052;
    gst_rtsp_message_unset(&resp);
    contextId = 1005053;

    return 1;
on_error:
    contextId = 1005054;
    gst_rtsp_message_unset(&req);
    contextId = 1005055;
    gst_rtsp_message_unset(&resp);
    contextId = 1005056;
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
        pu_log(LL_ERROR, "getWowzaConnSocket: the gst_rtsp_connection_get_write_socket() return NULL");
        return -1;
    }
    return g_socket_get_fd(gsock);
}