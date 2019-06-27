/*
 *  Copyright 2018 People Power Company
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
 * Created by Mikhail Levitin
 * JSON parser for scripting
 */
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include "cJSON.h"


 int main(int argc, char **argv)
{
    char* cfg_t = NULL;
    cJSON *cfg=NULL;
    size_t ptr = 0;
    char buffer[100];

    if (argv[1] == NULL) {
        fprintf(stderr,"You must specify key.\n");
        return 1;
    }
    while (fgets(buffer, sizeof(buffer), stdin)) {
        cfg_t = realloc(cfg_t, strlen(buffer)+ptr);
        memcpy(cfg_t+ptr, buffer, strlen(buffer));
        ptr += strlen(buffer);
    }
    cfg_t = realloc(cfg_t, ptr+1);
    cfg_t[ptr] = '\0';

    cfg = cJSON_Parse(cfg_t);
    if(cfg == NULL) {
        fprintf(stderr, "Error parsing the following:%s\n Error starts: %s", cfg_t, cJSON_GetErrorPtr());
        return 1;
    }
    cJSON *obj=NULL;

    if(obj = cJSON_GetObjectItem(cfg, argv[1]), obj == NULL) {
        fprintf(stderr, "No parameter '%s' at '%s'", argv[1], cfg_t);
        fflush(stderr);
        cJSON_Delete(cfg);
        return 1;
    }
    else
        switch (obj->type) {
            case cJSON_String:
                    fprintf(stdout, "%s", obj->valuestring);
                    break;
            case cJSON_Number:
                    fprintf(stdout, "%d", obj->valueint);
                    break;
            case cJSON_Object:
            case cJSON_Array:
                fprintf(stdout, "Unsopported object Array/Object");
                return 1;
        }
    cJSON_Delete(cfg);
    return 0;
}
