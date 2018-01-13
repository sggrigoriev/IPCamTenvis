
/*
 Created by gsg on 13/01/18.
 Here are parts of gstreamer rtsp sources from plugins.
 We got a little different tast - make proxy client, so we need just small part of all code.
*/

#ifndef IPCAMTENVIS_RTSP_MSG_EXT_H
#define IPCAMTENVIS_RTSP_MSG_EXT_H


#include <gst/rtsp/gstrtsp.h>

typedef struct _GstRTSPAuthCredential GstRTSPAuthCredential;
typedef struct _GstRTSPAuthParam GstRTSPAuthParam;

struct _GstRTSPAuthCredential {
    GstRTSPAuthMethod scheme;

    /* For Basic/Digest WWW-Authenticate and Digest
     * Authorization */
    GstRTSPAuthParam **params; /* NULL terminated */

    /* For Basic Authorization */
    gchar *authorization;
};

struct _GstRTSPAuthParam {
    gchar *name;
    gchar *value;
};

GstRTSPAuthCredential ** gst_rtsp_message_parse_auth_credentials (GstRTSPMessage * msg, GstRTSPHeaderField field);
void                     gst_rtsp_auth_credentials_free (GstRTSPAuthCredential ** credentials);

#endif /* IPCAMTENVIS_RTSP_MSG_EXT_H */