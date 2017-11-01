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
 Implements thread-safe ring buffer for video stream forwardring.
 NB! Limitations: properly worsk with only one read and only write threads!!!

 Buffer structure: array of elements. Each element is size_t size of chunk and byte array
 NB! The buffer works with 3rd party memory: putter allocates it and getter frees is
*/

#ifndef IPCAMTENVIS_AB_RING_BUFER_H
#define IPCAMTENVIS_AB_RING_BUFER_H

#include <stddef.h>

typedef unsigned char t_ab_byte;
typedef struct {
    size_t ls_size;
    t_ab_byte* data;
} t_ab_block;
typedef enum {
    AB_ERROR,       /* Data was not written. */
    AB_OK,          /* Data in buffer */
    AB_OVFERFLOW   /* Buffer overflown, data rewrires non-readed block */
} t_ab_put_rc;

/****************************************
 * Initialise ring buffer.
 * NB1. Total size in bytes = max_cunks*chunk_size + ((sizeof(size_t)+(sizeof(t_ab_byre*))*max_chunks)
 * NB2. Buffer can not take more than chunk_size bytes in one shot!
 * @param max_chunks - max amount of chunks stored ar one time
 * @return - 1 if OK, 0 if not (worng input params or memory allocation problems
 */
int ab_init(size_t max_chunks);
/****************************************
 * Closes the ring buffer
 */
void ab_close();
/****************************************
 * Get block for read. If nothing to read - wait until the info or timeout.
 * In case of timeout returns zero data: {0, NULL}
 * @param - to_sec - timeout in seconds waiting for data
 * @return pointer to data and size. NB! The reader will be responsible for block memory erase!
 */
const t_ab_block ab_getBlock(unsigned long to_sec);
/*****************************************
 * Put the block with the data into buffer
 * @param data_size - data size in bytes. NB! max allowed data size is chunk_size (see ab_init() parameters)
 * @param data  pointer to the data to be saved
 * @return t_ab_put_rc (see the description on t_ab_put_rc)
 */
t_ab_put_rc ab_putBlock(size_t data_size, t_ab_byte* data);



#endif /* IPCAMTENVIS_AB_RING_BUFER_H */
