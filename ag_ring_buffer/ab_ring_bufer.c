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
static int nowait = 0 ;
static size_t buf_len = 0;

static t_ab_block* buffer = NULL;

static size_t read_index;
static size_t write_index;
static volatile size_t valid_data_amt;

static volatile int is_data = 0;
static pthread_mutex_t data_available_cond_mutex;
static pthread_cond_t data_available_cond;
static pthread_mutex_t rw_index_guard;

static int buf_initialized = 0;

static const t_ab_block zero_elem = {0l, 0, NULL};


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
/********************************************************
 * Wait until smth will be writtem into the buffer
 * @param to_sec - timeout in seconds. If timeout == 0 - wait forever
 * @return - 1 if OK, 0 if timeout
 */
static int wait_for_data(unsigned long to_sec);

static t_ab_block non_blocking_read() {
    if(!valid_data_amt) return zero_elem;
    t_ab_block ret = buffer[read_index];
    read_index = (read_index+1)%buf_len;
    valid_data_amt -= 1;
    return ret;
}
static t_ab_put_rc non_blocking_write(t_ab_block* blk) {
    if(valid_data_amt >= buf_len) return AB_OVFERFLOW;
    valid_data_amt += 1;
    buffer[write_index] = *blk;
    write_index = (write_index+1)%buf_len;
    return AB_OK;
}

/**********************************************************
 * Pubic functions definition
 */
int ab_init(int no_block, size_t max_chunks) {
    nowait = no_block;
    if(buf_initialized) {
        pu_log(LL_ERROR, "ab_init: Ring buffer already initialized.");
        return 0;
    }
    if(max_chunks < 3) {
        pu_log(LL_ERROR, "ab_init: Buffer initialized with one data chunk. No R/W concurrency allowed!");
        return 0;
    }
    pthread_mutex_init(&data_available_cond_mutex, NULL);
    pthread_mutex_init(&rw_index_guard, NULL);
    pthread_cond_init(&data_available_cond, NULL);

    buf_len = max_chunks;
    is_data = 0;
    valid_data_amt = 0;

    read_index = 0;
    write_index = 0;

    buffer = (t_ab_block*) calloc(buf_len, sizeof(t_ab_block));
    if(!buffer) {
        pu_log(LL_ERROR, "ab_init: Ring buffer memory allocation error");
        return 0;
    }
    memset(buffer, 0, buf_len * sizeof(t_ab_block));
    buf_initialized = 1;

    return 1;
}
void ab_close() {
    if(!buf_initialized) return;
    buf_initialized = 0;

    while(read_index != write_index) {
        free(buffer[read_index].data);
        buffer[read_index] = zero_elem;
        read_index = shift(read_index);
    }

    free(buffer);
    buffer = NULL;
}
const t_ab_block ab_getBlock(unsigned long to_sec) {
    if(nowait) return non_blocking_read();

    t_ab_block ret = zero_elem;

    if(!is_data) {                              /* To prevent wait call for most cases. */
        if(!wait_for_data(to_sec)) return ret;   /* timeout */
    }

    pthread_mutex_lock(&rw_index_guard);
        ret = buffer[read_index];
        buffer[read_index] = zero_elem;
        read_index = shift(read_index);

        if(read_index == write_index) {
            pthread_mutex_lock(&data_available_cond_mutex);
                is_data = 0;
            pthread_mutex_unlock(&data_available_cond_mutex);
        }

    pthread_mutex_unlock(&rw_index_guard);
//    pu_log(LL_DEBUG, "%s: getBock data = %d, wr = %lu, rd = %lu\t", __FUNCTION__, is_data, write_index, read_index);
    return ret;
}
t_ab_put_rc ab_putBlock(t_ab_block* blk) {
    if(nowait) return non_blocking_write(blk);

    t_ab_put_rc ret = AB_OK;

    pthread_mutex_lock(&rw_index_guard);
        if(buffer[write_index].data) {      //Overflow case - we rewrite someone else's data
            ret = AB_OVFERFLOW;
            free(buffer[write_index].data); //The data will be never received so it has to be freed here
            buffer[write_index].data = NULL;
        }
        buffer[write_index] = *blk;

        write_index = shift(write_index);

    if(!is_data) {
            /* send the signal we got smth */
        pthread_mutex_lock(&data_available_cond_mutex);
            is_data = 1;
        pthread_cond_broadcast(&data_available_cond);           /* Who is the first - owns the slippers! */
        pthread_mutex_unlock(&data_available_cond_mutex);
        }
//        pu_log(LL_DEBUG, "%s: putBock data = %d, wr = %lu, rd = %lu\t", __FUNCTION__, is_data, write_index, read_index);

   pthread_mutex_unlock(&rw_index_guard);
    return ret;
}

/***********************************************************
 * Local fubcions implementation
 */
static volatile size_t shift(volatile size_t index) {
    return (index == (buf_len-1))?0:index+1;
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