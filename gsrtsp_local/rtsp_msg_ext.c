/*
 Copypasted from https://github.com/alessandrod/gst-plugins-base/blob/master/gst-libs/gst/rtsp/gstrtspmessage.c
 by gsg on 13/01/18.
*/

#include "rtsp_msg_ext.h"

static const gchar *
skip_lws (const gchar * s)
{
    while (g_ascii_isspace (*s))
        s++;
    return s;
}

static const gchar *
skip_commas (const gchar * s)
{
    /* The grammar allows for multiple commas */
    while (g_ascii_isspace (*s) || *s == ',')
        s++;
    return s;
}

static const gchar *
skip_scheme (const gchar * s)
{
    while (*s && !g_ascii_isspace (*s))
        s++;
    return s;
}

static const gchar *
skip_item (const gchar * s)
{
    gboolean quoted = FALSE;

    /* A list item ends at the last non-whitespace character
     * before a comma which is not inside a quoted-string. Or at
     * the end of the string.
     */
    while (*s) {
        if (*s == '"') {
            quoted = !quoted;
        } else if (quoted) {
            if (*s == '\\' && *(s + 1))
                s++;
        } else {
            if (*s == ',' || g_ascii_isspace (*s))
                break;
        }
        s++;
    }

    return s;
}

static void
decode_quoted_string (gchar * quoted_string)
{
    gchar *src, *dst;

    src = quoted_string + 1;
    dst = quoted_string;
    while (*src && *src != '"') {
        if (*src == '\\' && *(src + 1))
            src++;
        *dst++ = *src++;
    }
    *dst = '\0';
}

static void
parse_auth_credentials (GPtrArray * auth_credentials, const gchar * header,
                        GstRTSPHeaderField field)
{
    while (header[0] != '\0') {
        const gchar *end;
        GstRTSPAuthCredential *auth_credential;

        /* Skip whitespace at the start of the string */
        header = skip_lws (header);
        if (header[0] == '\0')
            break;

        /* Skip until end of string or whitespace: end of scheme */
        end = skip_scheme (header);

        auth_credential = g_new0 (GstRTSPAuthCredential, 1);

        if (g_ascii_strncasecmp (header, "basic", 5) == 0) {
            auth_credential->scheme = GST_RTSP_AUTH_BASIC;
        } else if (g_ascii_strncasecmp (header, "digest", 6) == 0) {
            auth_credential->scheme = GST_RTSP_AUTH_DIGEST;
        } else {
            /* Not supported, skip */
            g_free (auth_credential);
            header = end;
            continue;
        }

        /* Basic Authorization request has only an unformated blurb following, all
         * other variants have comma-separated name=value pairs */
        if (end[0] != '\0' && field == GST_RTSP_HDR_AUTHORIZATION
            && auth_credential->scheme == GST_RTSP_AUTH_BASIC) {
            auth_credential->authorization = g_strdup (end + 1);
            header = end;
        } else if (end[0] != '\0') {
            GPtrArray *params;

            params = g_ptr_array_new ();

            /* Space or start of param */
            header = end;

            /* Parse a header whose content is described by RFC2616 as
             * "#something", where "something" does not itself contain commas,
             * except as part of quoted-strings, into a list of allocated strings.
             */
            while (*header) {
                const gchar *item_end;
                const gchar *eq;

                header = skip_commas (header);
                item_end = skip_item (header);

                for (eq = header; *eq != '\0' && *eq != '=' && eq < item_end; eq++);
                if (eq[0] == '=') {
                    GstRTSPAuthParam *auth_param = g_new0 (GstRTSPAuthParam, 1);
                    const gchar *value;

                    /* have an actual param */
                    auth_param->name = g_strndup (header, eq - header);

                    value = eq + 1;
                    value = skip_lws (value);
                    auth_param->value = g_strndup (value, item_end - value);
                    if (value[0] == '"')
                        decode_quoted_string (auth_param->value);

                    g_ptr_array_add (params, auth_param);
                    header = item_end;
                } else {
                    /* at next scheme, header at start of it */
                    break;
                }
            }
            if (params->len)
                g_ptr_array_add (params, NULL);
            auth_credential->params =
                    (GstRTSPAuthParam **) g_ptr_array_free (params, FALSE);
        } else {
            header = end;
        }
        g_ptr_array_add (auth_credentials, auth_credential);

        /* WWW-Authenticate allows multiple, Authorization allows one */
        if (field == GST_RTSP_HDR_AUTHORIZATION)
            break;
    }
}


/**
 * gst_rtsp_message_parse_auth_credentials:
 * @msg: a #GstRTSPMessage
 * @field: a #GstRTSPHeaderField
 *
 * Parses the credentials given in a WWW-Authenticate or Authorization header.
 *
 * Returns: %NULL-terminated array of GstRTSPAuthCredential or %NULL.
 *
 * Since: 1.12
 */
GstRTSPAuthCredential **
gst_rtsp_message_parse_auth_credentials (GstRTSPMessage * msg,
                                         GstRTSPHeaderField field)
{
    gchar *header;
    GPtrArray *auth_credentials;
    gint i;

    g_return_val_if_fail (msg != NULL, NULL);

    auth_credentials = g_ptr_array_new ();

    i = 0;
    while (gst_rtsp_message_get_header (msg, field, &header, i) == GST_RTSP_OK) {
        parse_auth_credentials (auth_credentials, header, field);
        i++;
    }

    if (auth_credentials->len)
        g_ptr_array_add (auth_credentials, NULL);

    return (GstRTSPAuthCredential **) g_ptr_array_free (auth_credentials, FALSE);
}

#ifdef GST_EXT
static void     /* NB! In the source it was global function I beleine*/
#else
void     /* NB! In the source it was global function */
#endif
gst_rtsp_auth_param_free (GstRTSPAuthParam * param)
{
    if (param != NULL) {
        g_free (param->name);
        g_free (param->value);
        g_free (param);
    }
}

static void
gst_rtsp_auth_credential_free (GstRTSPAuthCredential * credential)
{
    GstRTSPAuthParam **p;

    if (credential == NULL)
        return;

    for (p = credential->params; p != NULL && *p != NULL; ++p)
        gst_rtsp_auth_param_free (*p);

    g_free (credential->params);
    g_free (credential->authorization);
    g_free (credential);
}

/**
 * gst_rtsp_auth_credentials_free:
 * @credentials: a %NULL-terminated array of #GstRTSPAuthCredential
 *
 * Free a %NULL-terminated array of credentials returned from
 * gst_rtsp_message_parse_auth_credentials().
 *
 * Since: 1.12
 */
void
gst_rtsp_auth_credentials_free (GstRTSPAuthCredential ** credentials)
{
    GstRTSPAuthCredential **p;

    if (!credentials)
        return;

    for (p = credentials; p != NULL && *p != NULL; ++p)
        gst_rtsp_auth_credential_free (*p);

    g_free (credentials);
}


