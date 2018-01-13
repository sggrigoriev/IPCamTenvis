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
 Created by gsg on 27/10/17.
*/

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "ab_ring_bufer.h"

pthread_t writer_id, reader_id;
pthread_attr_t writer_attr, reader_attr;

void* writer(void* v) {     /* Writer thiead */
    unsigned i = 0;
    while(1) {
        t_ab_byte* ar = malloc(3);
//        sleep(1);
        i++;
        ar[0] = i; ar[1] = i;ar[2] = i;
        switch (ab_putBlock(3, ar)) {
            case AB_ERROR:
                printf("writer: ab_putBlock return AB_ERROR\n");
                break;
            case AB_OK:
                printf("writer: ab_putBlock return AB_OK\n");
                break;
            case AB_OVFERFLOW:
//                printf("writer: ab_putBlock return AB_OVFERFLOW\n");
                break;
            default:
                printf("writer: ab_putBlock return something strange...\n");
                break;
        }
    }

}

void* reader(void* v) {     /* Reader thread */
    while(1) {
        sleep(1);
        t_ab_block ret = ab_getBlock(20);
        if(!ret.ls_size) {
            printf("reader: timeout! Nothing to read\n");
        }
        else {
            printf("reader: received %d\t%d\t%d\n", ret.data[0], ret.data[1], ret.data[2]);
            free(ret.data);
        }
    }
}

int main() {
    char* a = "v=0\r\n"
            "o=- 0 0 IN IP4 127.0.0.1\r\n"
            "s=\\11\r\n"
            "c=IN IP4 184.73.181.211\r\n"
            "t=0 0\r\n"
            "a=tool:libavformat 57.65.100\r\n"
            "m=video 0 RTP/AVP 96\r\n"
            "b=AS:150\r\n"
            "a=rtpmap:96 MP4V-ES/90000\n"
            "a=fmtp:96 profile-level-id=1; config=000001B002000001B58913000001000000012000C48D8800CD3C04871443000001B24C61766335372E37352E313030\r\n"
            "a=control:streamid=0\r\n"
            "m=audio 0 RTP/AVP 97\r\n"
            "b=AS:64\r\n"
            "a=rtpmap:97 MPEG4-GENERIC/48000/2\r\n"
            "a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3; config=119056E500\r\n"
            "a=control:streamid=1\r\n\r\n";

    size_t la = strlen(a);
    printf("%lu  ", la);
    exit(0);

    void *ret;
    ab_init(20);

    pthread_attr_init(&writer_attr);
    pthread_create(&writer_id, &writer_attr, &writer, NULL);

    pthread_attr_init(&reader_attr);
    pthread_create(&reader_id, &reader_attr, &reader, NULL);


    pthread_join(reader_id, &ret);
    pthread_attr_destroy(&reader_attr);

    pthread_join(writer_id, &ret);
    pthread_attr_destroy(&writer_attr);

    ab_close();
    return 0;
}

