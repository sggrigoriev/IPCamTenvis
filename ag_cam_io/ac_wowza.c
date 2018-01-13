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
#include "pu_logger.h"

#include "ag_defaults.h"
#include "ag_settings.h"
#include "au_string.h"
#include "rtsp_msg_ext.h"

#include "ac_wowza.h"

#define AC_GST_ANAL(a)    if((a) != GST_RTSP_OK) { \
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
        if(gs->conn) {
            gst_rtsp_connection_flush (gs->conn, TRUE);
            gst_rtsp_connection_close (gs->conn);
            gst_rtsp_connection_free(gs->conn);
        }
        if(gs->url) gst_rtsp_url_free (gs->url);
        if(gs->wowza_session) free(gs->wowza_session);
        free(gs);
    }
}

/* Replace
 * o=StreamingServer 3331435948 1116907222000 IN IP4 10.42.0.115 -> o=- 0 0 IN IP4 127.0.0.1
 * c=IN IP4 0.0.0.0 -> c=IN IP4 <WOWZA IP>
 * TODO: gst_sdp_message_parse_buffer ()!!!
*/
static char* make_announce_body(char* buf, size_t size, const char* cam_descr) {
    char ip[20] ={0};
    char connection[500] ={0};

    if(!au_strcpy(buf, cam_descr, size)) return 0;

    ag_getClientIP(ip, sizeof(ip));
    if(!au_getSection(connection, sizeof(connection), cam_descr, AC_RTSP_SDP_CD, AC_RTSP_EOL, AU_NOCASE)) {
        pu_log(LL_ERROR, "%s: can not extract connection parameter from Camera SDP %s Exiting", __FUNCTION__, cam_descr);
        return NULL;
    }
    if(!au_replaceSection(connection, sizeof(connection), AC_RTSP_CD_IP4, AC_RTSP_EOL, AU_NOCASE, ip)) {
        pu_log(LL_ERROR, "%s: can not replace IP %s in VS connection parameter %s Exiting", __FUNCTION__, ip, cam_descr);
        return NULL;
    }
    if(!au_replaceSection(buf, size, AC_RTSP_SDP_CD, AC_RTSP_EOL, AU_NOCASE, connection)) {
        pu_log(LL_ERROR, "%s: can not replace connection parameter %s to VS SDP %s Exiting", __FUNCTION__, connection, buf);
        return NULL;
    }

// Replace origin
    if(!au_replaceSection(buf, size, AC_RTSP_SDP_ORIGIN, AC_RTSP_EOL, AU_NOCASE, AC_RTSP_VS_ORIGIN)) {
        pu_log(LL_ERROR, "%s: can not replace origin parameter %s to VS SDP %s Exiting", __FUNCTION__, AC_RTSP_VS_ORIGIN, buf);
        return NULL;
    }
    return buf;
}

t_at_rtsp_session* ac_WowzaInit() {
    t_at_rtsp_session* ret = calloc(sizeof(t_at_rtsp_session), 1);
    if(!ret) {
        pu_log(LL_ERROR, "%s: Memory allocation error!", __FUNCTION__);
        return NULL;
    }

    ret->device = AC_WOWZA;

    GError* err = NULL;
    if(!gst_init_check (NULL, NULL, &err) || err != NULL) {
        pu_log(LL_ERROR, "%s: Error GST init: %s", __FUNCTION__, err->message);
        g_error_free (err);
        free(ret);
        return NULL;
    }

    return ret;
}
void ac_WowzaDown(t_at_rtsp_session* sess) {
    AT_DT_NR(sess->device, AC_WOWZA);
    gst_deinit();
    free(sess);
}

int ac_WowzaOpenSession(t_at_rtsp_session* sess, const char* wowza_session) {
    AT_DT_RT(sess->device, AC_WOWZA, 0);
    GstRTSPResult rc;
    GTimeVal conn_timeout = {3,0};

    if(!wowza_session) {
        pu_log(LL_ERROR, "%s: No WOWZA session passed", __FUNCTION__);
        return 0;
    }

    t_gst_session* gs = NULL;
    if(gs = calloc(sizeof(t_gst_session), 1), !gs) {
        pu_log(LL_ERROR, "%s: Memory allocation error!", __FUNCTION__);
        return 0;
    }
    rc = gst_rtsp_url_parse (sess->url, &gs->url);          // Make the object from conn string
    AC_GST_ANAL(rc);

    rc = gst_rtsp_connection_create(gs->url, &gs->conn);        //Create connection object using url object
    AC_GST_ANAL(rc);

    rc = gst_rtsp_connection_connect (gs->conn, &conn_timeout);  //Connect with timeout set
    AC_GST_ANAL(rc);

    GTimeVal io_to = {3,0};
    gs->io_to = io_to;
    gs->wowza_session = strdup(wowza_session);
    if(!gs->wowza_session) {
        pu_log(LL_ERROR, "%s: Memore allocation error at %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    sess->session = gs;
    sess->CSeq = 1;

    return 1;
on_error:
    clear_session(gs);
    return 0;
}
void ac_WowzaCloseSession(t_at_rtsp_session* sess) {
    AT_DT_NR(sess->device, AC_WOWZA);

    clear_session(sess->session);
    sess->session = NULL;
    sess->CSeq = 1;
    if(sess->rtsp_session_id) { free(sess->rtsp_session_id); sess->rtsp_session_id = NULL; }
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
    rc = gst_rtsp_message_get_header(&msg, GST_RTSP_HDR_CSEQ, &val, 0);
    rc = gst_rtsp_message_get_header(&msg, GST_RTSP_HDR_SERVER, &val, 0);
    rc = gst_rtsp_message_get_header(&msg, GST_RTSP_HDR_PUBLIC, &val, 0);
    gint i;
    typedef struct _RTSPKeyValue
    {
        GstRTSPHeaderField field;
        gchar *value;
    } RTSPKeyValue;

    for (i = 0; i < msg.hdr_fields->len; i++) {
        RTSPKeyValue *key_value = &g_array_index (msg.hdr_fields, RTSPKeyValue, i);
        if(key_value->field != GST_RTSP_HDR_INVALID) {
            pu_log(LL_DEBUG, "%s: Field = %s, Value = %s\n", __FUNCTION__, gst_rtsp_header_as_text(key_value->field), key_value->value);
        }
        else {
            pu_log(LL_WARNING, "%s: Field #%d is invalid. Exiting\n", __FUNCTION__, i);
            break;
        }
    }

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
    if(!make_announce_body(new_description, sizeof(new_description), description)) goto on_error;

    rc = gst_rtsp_message_init (&req); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_init_request (&req, GST_RTSP_ANNOUNCE, sess->url); AC_GST_ANAL(rc);
// Header
    rc = gst_rtsp_message_add_header (&req, GST_RTSP_HDR_CONTENT_TYPE, AC_RTSP_CONTENT_TYPE); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_CSEQ, num); AC_GST_ANAL(rc);
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_USER_AGENT, AC_RTSP_CLIENT_NAME); AC_GST_ANAL(rc);
    snprintf(num, sizeof(num)-1, "%lu", strlen(new_description));
    rc = gst_rtsp_message_add_header(&req, GST_RTSP_HDR_CONTENT_LENGTH, AC_RTSP_CLIENT_NAME); AC_GST_ANAL(rc);
// Body
    rc = gst_rtsp_message_set_body(&req, new_description, strlen(new_description));

    rc = gst_rtsp_connection_send (gs->conn, &req, &gs->io_to); AC_GST_ANAL(rc);    /* Send */

    rc = gst_rtsp_message_init (&resp); AC_GST_ANAL(rc);
    rc = gst_rtsp_connection_receive (gs->conn, &resp, &gs->io_to); AC_GST_ANAL(rc); /* Receive */

    if(resp.type != GST_RTSP_MESSAGE_RESPONSE) {
        pu_log(LL_ERROR, "%s: wrong GST message type %d. Expected one is %d", __FUNCTION__, resp.type, GST_RTSP_MESSAGE_RESPONSE);
        goto on_error;
    }
    if(resp.type_data.response.code != GST_RTSP_STS_UNAUTHORIZED) {
        pu_log(LL_ERROR, "%s: bad answer: %s", gst_rtsp_status_as_text(resp.type_data.response.code));
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

    sess->CSeq++;

    gst_rtsp_message_unset(&req);
    gst_rtsp_message_unset(&resp);

    return 1;
on_error:
    gst_rtsp_message_unset(&req);
    gst_rtsp_message_unset(&resp);
    return 0;
}
int ac_WowzaSetup(t_at_rtsp_session* sess, int client_port) {
    AT_DT_RT(sess->device, AC_WOWZA, 0);
    GstRTSPResult rc;
    t_gst_session* gs = sess->session;
    GstRTSPMessage msg = {0};
    char num[10];
    snprintf(num, sizeof(num)-1, "%d", sess->CSeq);

    rc = gst_rtsp_message_init (&msg); AC_GST_ANAL(rc);
//1. Add trackId = 1 to the url;
    char tmp_url[AC_RTSP_MAX_URL_SIZE] = {0};
    snprintf(tmp_url, sizeof(tmp_url), "%s/%s0", sess->url, AC_TRACK);
    free(sess->url);
    sess->url = strdup(tmp_url);
    if(!sess->url) {
        pu_log(LL_ERROR, "%s: Memory allocation error at %d", __FUNCTION__, __LINE__);
        goto on_error;
    }
    rc = gst_rtsp_message_init_request (&msg, GST_RTSP_SETUP, sess->url); AC_GST_ANAL(rc);

//2. Add transport header
    GstRTSPTransport *transport = NULL;
    rc = gst_rtsp_transport_new (&transport); AC_GST_ANAL(rc);

    transport->client_port.min = client_port;
    transport->client_port.max = client_port+1;
    transport->lower_transport = GST_RTSP_LOWER_TRANS_UDP;
    transport->mode_play = TRUE;
//    gs->transport->mode_record = TRUE;
    transport->profile = GST_RTSP_PROFILE_AVP;
    transport->trans = GST_RTSP_TRANS_RTP;

    gchar* text_transport = gst_rtsp_transport_as_text (transport);
    rc = gst_rtsp_message_add_header (&msg, GST_RTSP_HDR_TRANSPORT, text_transport); AC_GST_ANAL(rc);
    g_free(text_transport);
    gst_rtsp_transport_free(transport);

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
    rc = gst_rtsp_transport_parse(text_transport, transport); AC_GST_ANAL(rc);
//Save server port
    transport->server_port.min




    gst_rtsp_message_unset(&msg);
on_error:
    if(transport) gst_rtsp_transport_free(transport);
    gst_rtsp_message_unset(&msg);
    return 0;
}
int ac_WowzaPlay(t_at_rtsp_session* sess) {
    return 0;
}
int ac_WowzaTeardown(t_at_rtsp_session* sess) {
    return 0;
}
/*
static const char* make_announce_answer(const char* describe_body, char* header, size_t size) {
    char ip[20] ={0};
    char connection[500] ={0};

    if(!au_strcpy(header, describe_body, size)) return 0;

// Replace IP in c=
    ag_getClientIP(ip, sizeof(ip));
    if(!au_getSection(connection, sizeof(connection), describe_body, AC_RTSP_SDP_CD, AC_RTSP_EOL, AU_NOCASE)) {
        pu_log(LL_ERROR, "%s: can not extract connection parameter from Camera SDP %s Exiting", __FUNCTION__, describe_body);
        return NULL;
    }
    if(!au_replaceSection(connection, sizeof(connection), AC_RTSP_CD_IP4, AC_RTSP_EOL, AU_NOCASE, ip)) {
        pu_log(LL_ERROR, "%s: can not replace IP %s in VS connection parameter %s Exiting", __FUNCTION__, ip, describe_body);
        return NULL;
    }
    if(!au_replaceSection(header, size, AC_RTSP_SDP_CD, AC_RTSP_EOL, AU_NOCASE, connection)) {
        pu_log(LL_ERROR, "%s: can not replace connection parameter %s to VS SDP %s Exiting", __FUNCTION__, connection, header);
        return NULL;
    }

// Replace origin
    if(!au_replaceSection(header, size, AC_RTSP_SDP_ORIGIN, AC_RTSP_EOL, AU_NOCASE, AC_RTSP_VS_ORIGIN)) {
        pu_log(LL_ERROR, "%s: can not replace origin parameter %s to VS SDP %s Exiting", __FUNCTION__, AC_RTSP_VS_ORIGIN, header);
        return NULL;
    }
    return header;
}

// WWW-Authenticate: Digest realm="<realm>",  nonce="<nonce>"
static int get_challenge_params(const char* auth, char* realm, size_t r_size, char* nonce, size_t n_size) {
    int auth_start;

    if(auth_start = au_findSubstr(auth, AC_RTSP_CHALLENGE_HEAD, AU_NOCASE), auth_start < 0) {
        pu_log(LL_ERROR, "%s: %s is not found", __FUNCTION__, AC_RTSP_CHALLENGE_HEAD);
        return 0;
    }
    if(!au_getSection(realm, r_size, auth+auth_start, AC_RTSP_F_DIGEST_REALM, "\",", AU_NOCASE)) {
        pu_log(LL_ERROR, "%s: Can't find REALM value from %s", __FUNCTION__, auth);
        return 0;
    }
    if(!au_getSection(nonce, n_size, auth+auth_start, AC_RTSP_F_DIGEST_NONCE, "\""AC_RTSP_EOL, AU_NOCASE)) {
        pu_log(LL_ERROR, "%s: Can't find NONCE value from %s", __FUNCTION__, auth);
        return 0;
    }
    return 1;
}
// Session: 229498000;timeout=60
static int get_session_params(const char* ans, char* session_id, size_t s_size, unsigned int* to) {
    char d[20]={0};
    if(!au_getSection(session_id, s_size, ans, AC_SESSION, ";", AU_NOCASE)) return 0;
    if(!au_getSection(d, sizeof(d), ans, AC_TIMEOUT, NULL, AU_NOCASE)) return 0;
    *to = atoi(d);
    return 1;
}

// Authorization: Digest username="<uname>", realm="<realm>", nonce="<nonce>", uri="<uri>", response="<response>"
static const char* make_auth_header(char* auth, size_t size) {
    if(!au_strcpy(vs_auth, AC_RTSP_DIGEST_AUTH, size)) goto on_error;

    if(!au_strcat(auth, AC_RTSP_DIGEST_UNAME, size)) goto on_error;
    if(!au_strcat(auth, "\"", size)) goto on_error;
    if(!au_strcat(auth, vs.vs_session_id, size)) goto on_error;
    if(!au_strcat(auth, "\", ", size)) goto on_error;

    if(!au_strcat(auth, AC_RTSP_DIGEST_REALM, size)) goto on_error;
    if(!au_strcat(auth, "\"", size)) goto on_error;
    if(!au_strcat(auth, vs_realm, size)) goto on_error;
    if(!au_strcat(auth, "\", ", size)) goto on_error;

    if(!au_strcat(auth, AC_RTSP_DIGEST_NONCE, size)) goto on_error;
    if(!au_strcat(auth, "\"", size)) goto on_error;
    if(!au_strcat(auth, vs_nonce, size)) goto on_error;
    if(!au_strcat(auth, "\", ", size)) goto on_error;

    if(!au_strcat(auth, AC_RTSP_DIGEST_URI, size)) goto on_error;
    if(!au_strcat(auth, "\"", size)) goto on_error;
    if(!au_strcat(auth, vs.url, size)) goto on_error;
    if(!au_strcat(auth, "\", ", size)) goto on_error;

    char response[100]={0};
    if(!ag_digest_make_response(response, sizeof(response))) goto on_error;
    if(!au_strcat(auth, AC_RTSP_DIGEST_RESPONSE, size)) goto on_error;
    if(!au_strcat(auth, "\"", size)) goto on_error;
    if(!au_strcat(auth, response, size)) goto on_error;
    if(!au_strcat(auth, "\"", size)) goto on_error;

    return auth;
    on_error:
    pu_log(LL_ERROR, "%s: Error creating DIGEST auth header", __FUNCTION__);
    return NULL;
}
*/
const char* ac_make_wowza_url(char *url, size_t size, const char* vs_url, int port, const char* vs_session_id) {
    char s_port[20];
    url[0] = '\0';
    sprintf(s_port, "%d", port);

    if((strlen(vs_url)+strlen(s_port)+strlen(vs_session_id)+strlen(DEFAULT_PPC_VIDEO_FOLDER) + 4) > (size-1)) {
        pu_log(LL_ERROR, "%s: buffer size too low. VS URL can't be constructed", __FUNCTION__);
        return url;
    }
    sprintf(url, "%s:%s/%s/%s", vs_url, s_port, DEFAULT_PPC_VIDEO_FOLDER, vs_session_id);
    return url;

}