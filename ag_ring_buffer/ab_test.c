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

#include "ab_ring_bufer.h"

pthread_t writer_id, reader_id;
pthread_attr_t writer_attr, reader_attr;

void* writer(void* v) {     /* Writer thiead */
    unsigned i = 0;
    while(1) {
        t_ab_byte ar[3];
 //       sleep(1);
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
                printf("writer: ab_putBlock return AB_OVFERFLOW\n");
                break;
            case AB_TRUNCATED:
                printf("writer: ab_putBlock return AB_TRUNCATED\n");
                break;
            default:
                printf("writer: ab_putBlock return something strange...\n");
                break;
        }
    }

}

void* reader(void* v) {     /* Reader thread */
    while(1) {
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
    void *ret;

    ab_init(2, 10);

    pthread_attr_init(&reader_attr);
    pthread_create(&reader_id, &reader_attr, &reader, NULL);

//    sleep(60);

    pthread_attr_init(&writer_attr);
    pthread_create(&writer_id, &writer_attr, &writer, NULL);

    pthread_join(reader_id, &ret);
    pthread_attr_destroy(&reader_attr);

    pthread_join(writer_id, &ret);
    pthread_attr_destroy(&writer_attr);

    ab_close();
    return 0;
}

