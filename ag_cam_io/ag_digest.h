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
 Responsible for DIGEST auth
*/

#ifndef IPCAMTENVIS_AG_DIGEST_H
#define IPCAMTENVIS_AG_DIGEST_H

#include <stdio.h>

int ag_digest_init();
void ag_digest_destroy();

int ag_digest_start(const char* uname, const char* password, const char* realm, const char* nonce, const char* method, const char* url);
const char* ag_digest_make_response(char* buf, size_t size);


#endif /* IPCAMTENVIS_AG_DIGEST_H */
