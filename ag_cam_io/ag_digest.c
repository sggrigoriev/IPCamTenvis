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
 Created by gsg on 21/12/17.
*/

#include <openssl/evp.h>
#include <assert.h>
#include <string.h>

#include "au_string.h"
#include "ag_digest.h"


static const EVP_MD *md;

static char* un=NULL;
static char* p=NULL;
static char* r=NULL;
static char* n=NULL;
static char* m=NULL;
static char* u=NULL;

static const char* AI(char* buf, size_t size, const char* p1, const char* p2, const char* p3) {
/*
    unsigned char resp[EVP_MAX_MD_SIZE];
    unsigned int resp_len;
    EVP_MD_CTX mdctx;

    EVP_MD_CTX_init(&mdctx);
    EVP_DigestInit_ex(&mdctx, md, NULL);
    EVP_DigestUpdate(&mdctx, p1, strlen(p1));
    EVP_DigestUpdate(&mdctx, ":", 1);
    EVP_DigestUpdate(&mdctx, p2, strlen(p2));
    if(strlen(p3)) {
        EVP_DigestUpdate(&mdctx, ":", 1);
        EVP_DigestUpdate(&mdctx, p3, strlen(p3));
    }

    EVP_DigestFinal_ex(&mdctx, resp, &resp_len);
    EVP_MD_CTX_cleanup(&mdctx);

    if (!au_bytes_2_hex_str(buf, resp, resp_len, size)) return NULL;
*/
    return buf;
}

int ag_digest_init() {
/*
    OpenSSL_add_all_digests();

    md = EVP_md5();

    un = NULL; p = NULL; r = NULL; n = NULL; m = NULL; u = NULL;
*/
    return 1;
}
void ag_digest_destroy() {
/*
    if(un) free(un);
    if(p) free(p);
    if(r) free(r);
    if(n) free(n); if(m) free(m); if(u) free(u);
*/
}
int ag_digest_start(const char* uname, const char* password, const char* realm, const char* nonce, const char* method, const char* url) {
/*
    assert(uname); assert(password); assert(realm); assert(nonce); assert(method); assert(url);
    un = strdup(uname); p = strdup(password); r = strdup(realm), n = strdup(nonce); m = strdup(method); u = strdup(url);
*/
    return 1;
}
const char* ag_digest_make_response(char* buf, size_t size) {
/*
    char A1[EVP_MAX_MD_SIZE*2+1];  // Max len in symbols + 0-byte
    char A2[EVP_MAX_MD_SIZE*2+1];

    if(!AI(A1, sizeof(A1), un, r, p)) return NULL;
    if(!AI(A2, sizeof(A2), m, u, "")) return NULL;
    return AI(buf, size, A1, n, A2);
*/
    return NULL;
}

