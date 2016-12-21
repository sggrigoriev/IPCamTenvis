//
// Created by gsg on 15/12/16.
//

#ifndef PRESTO_LIB_HTTP_H
#define PRESTO_LIB_HTTP_H

#include <stdlib.h>

#define LIBB_HTTP_MAX_POST_RETRIES  3
#define LIB_HTTP_MAX_MSG_SIZE   8192
#define LIB_HTTP_MAX_URL_SIZE   4097
#define LIB_HTTP_AUTHENTICATION_STRING_SIZE 128
#define LIB_HTTP_DEVICE_ID_SIZE 31
//Proxy will send to the Server connection timeout less on LIB_HTTP_PROXY_SERVER_TO_DELTA sec during the long GET
#define LIB_HTTP_PROXY_SERVER_TO_DELTA 20
/** Maximum time for an HTTP connection, including name resolving */
#define LIB_HTTP_DEFAULT_CONNECT_TIMEOUT_SEC 30
/** Maximum time for an HTTP transfer */
#define LIB_HTTP_DEFAULT_TRANSFER_TIMEOUT_SEC 60

typedef enum{LIB_HTTP_POST_ERROR = -1, LIB_HTTP_POST_RETRY = 0, LIB_HTTP_POST_OK = 1, LIB_HTTP_POST_AUTH_TOKEN = 2} lib_http_post_result_t;

//return 1 of OK, 0 if not
int lib_http_init();
void lib_http_close();

//Return 1 if OK, 0 if error.All errors put into log inside
int lib_http_create_get_persistent_conn(const char *url, const char* auth_token, const char* deviceID, unsigned int conn_to);
void ilb_http_delete_get_persistent_conn();

//Return 1 if get msg, 0 if timeout, -1 if error. Logged inside
int lib_http_get(char* msg, size_t msg_len);

//Return 1 if OK, 0 if timeout, -1 if error. if strlen(relpy)>0 look for some answer from the server. All logging inside
//Makes new connectione every time to post amd close it after the operation
lib_http_post_result_t lib_http_post(const char* msg, char* reply, size_t reply_size, const char* url, const char* auth_token, const char* deviceID);

#endif //PRESTO_LIB_HTTP_H