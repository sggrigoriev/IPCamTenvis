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

#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <malloc.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>


#include "pu_logger.h"

#include "ab_ring_bufer.h"

#define AB_TIMEOUT  0


/**********************************************************
 * Local data types & data
 */
static size_t buf_len = 0;
static size_t chunk_len;

static t_ab_block* buffer = NULL;

static volatile size_t read_index;
static volatile size_t write_index;

static volatile int is_data = 0;
static pthread_mutex_t data_available_cond_mutex;
static pthread_cond_t data_available_cond;
static pthread_mutex_t rw_index_guard;

static int buf_initialized = 0;


/**********************************************************
 * Local functions definition
 */
/******************************************************
 * Shift the index to right on ring buffer
 * NB! thread-protected
 * @param index - shifting value
 * @param lr - if lr == 0 - left shift else right shift
 * @return - changed value
 */
static volatile size_t shift(volatile size_t index);
/******************************************************
 * Write data to the destination
 * @param dest - pointer to the destination
 * @param data - data to write
 * @param size - data size
 * @return - 1 if OK, 0 if error
 */
static int put_data(t_ab_block* dest, const t_ab_byte* data, size_t size);
/********************************************************
 * Wait until smth will be writtem into the buffer
 * @param to_sec - timeout in seconds. If timeout == 0 - wait forever
 * @return - 1 if OK, 0 if timeout
 */
static int wait_for_data(unsigned long to_sec);

/**********************************************************
 * Pubic functions definition
 */
int ab_init(size_t max_chunks, size_t chunk_size) {
    if(buf_initialized) {
        pu_log(LL_ERROR, "ab_init: Ring buffer already initialized.");
        return 0;
    }
    if(max_chunks < 2) {
        pu_log(LL_ERROR, "ab_init: Buffer initialized with one data chunk. No R/W concurrency allowed!");
        return 0;
    }
    if(!chunk_size) {
        pu_log(LL_ERROR, "ab_init: Zero chunk size. Can't work.");
        return 0;
    }

    pthread_mutex_init(&data_available_cond_mutex, NULL);
    pthread_mutex_init(&rw_index_guard, NULL);
    pthread_cond_init(&data_available_cond, NULL);

    buf_len = max_chunks;
    chunk_len = chunk_size;
    is_data = 0;

    read_index = 0;
    write_index = 0;

    buffer = (t_ab_block*) malloc(buf_len * sizeof(t_ab_block));
    if(!buffer) {
        pu_log(LL_ERROR, "ab_init: Ring buffer memory allocation error");
        return 0;
    }
    memset(buffer, 0, buf_len * sizeof(t_ab_block));
    buf_initialized = 1;

    return 1;
}
void ab_close() {
    assert(buf_initialized);
    buf_initialized = 0;
    size_t i;
    for(i = 0; i < buf_len; i++) {
        if(buffer[i].data) free(buffer[i].data);
    }
    free(buffer);
    buffer = NULL;
}
const t_ab_block ab_getBlock(unsigned long to_sec) {
    assert(buf_initialized);
    t_ab_block ret = {0l, NULL};

    if(!is_data) {                              /* To prevent wait call for most cases. */
        if(!wait_for_data(to_sec)) return ret;   /* timeout */
    }
    pthread_mutex_lock(&rw_index_guard);
        ret = buffer[read_index];
        read_index = shift(read_index);
        pthread_mutex_lock(&data_available_cond_mutex);
            is_data = (read_index != write_index);
        pthread_mutex_unlock(&data_available_cond_mutex);
//    printf("getBock data = %d, wr = %lu, rd = %lu\t", is_data, write_index, read_index);
    pthread_mutex_unlock(&rw_index_guard);
    return ret;
}
t_ab_put_rc ab_putBlock(size_t data_size, const t_ab_byte* data) {
    assert(buf_initialized);
    assert(data_size);
    assert(data);

    t_ab_put_rc ret = AB_OK;
    t_ab_block* ptr;

    if(data_size > chunk_len) {
        data_size = chunk_len;
        ret = AB_TRUNCATED;
    }
    pthread_mutex_lock(&rw_index_guard);
        if(!is_data) {
            /* send the signal we got smth */
            pthread_mutex_lock(&data_available_cond_mutex);
            is_data = 1;
            pthread_cond_broadcast(&data_available_cond);           /* Who is the first - owns the slippers! */
            pthread_mutex_unlock(&data_available_cond_mutex);
            write_index = shift(write_index);
        }
        else if(shift(write_index) != read_index) {     /* There is an empty space to write */
            write_index = shift(write_index);
        }
        else {                                          /* Overflow */
            ret = AB_OVFERFLOW;
            write_index = shift(read_index);    /* the non-red data will be overriden */
        }
        ptr = buffer + write_index;
//        printf("putBock data = %d, wr = %lu, rd = %lu\t", is_data, write_index, read_index);
    pthread_mutex_unlock(&rw_index_guard);
    if(!put_data(ptr, data, data_size)) ret = AB_ERROR;
    return ret;
}

/***********************************************************
 * Local fubcions implementation
 */
static volatile size_t shift(volatile size_t index) {
    return (index == (buf_len-1))?0:index+1;
}
static int put_data(t_ab_block* dest, const t_ab_byte* data, size_t size) {
    dest->ls_size = size;
    dest->data = malloc(size);
    if(!dest->data) return 0;
    memcpy(dest->data, data, size);
    return 1;
}
static int wait_for_data(unsigned long to_sec) {
    struct timespec timeToWait;
    struct timeval now;
    int rt;

    gettimeofday(&now, NULL);

    timeToWait.tv_sec = now.tv_sec+to_sec;
    timeToWait.tv_nsec = 0;

    pthread_mutex_lock(&data_available_cond_mutex);
        if(to_sec) {
            rt = pthread_cond_timedwait(&data_available_cond, &data_available_cond_mutex, &timeToWait);
            if (rt == ETIMEDOUT) {
                pthread_mutex_unlock(&data_available_cond_mutex);
                return AB_TIMEOUT;
            }
        }
        else {
            pthread_cond_wait(&data_available_cond, &data_available_cond_mutex);
        }
    pthread_mutex_unlock(&data_available_cond_mutex);
    return 1;
}