
/*
 Created by gsg on 13/01/18.
 Here are parts of gstreamer rtsp sources from plugins.
 We got a little different tast - make proxy client, so we need just small part of all code.
*/

#ifndef IPCAMTENVIS_RTSP_MSG_EXT_H
#define IPCAMTENVIS_RTSP_MSG_EXT_H


#include <gst/gstconfig.h>
#include <gst/rtsp/gstrtsp.h>
#include <gst/rtsp/gstrtspmessage.h>

//typedef struct _GstRTSPAuthCredential GstRTSPAuthCredential;
//typedef struct _GstRTSPAuthParam GstRTSPAuthParam;
#if 0
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
#else
#include <gstreamer-1.0/gst/rtsp/gstrtspmessage.h>
#endif
#endif /* IPCAMTENVIS_RTSP_MSG_EXT_H */
