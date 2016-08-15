/*
 *  Copyright 2013 People Power Company
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

#ifndef TIMESTAMP_H
#define TIMESTAMP_H

enum {
  TIMESTAMP_ZONE_SIZE = 8,
  TIMESTAMP_STAMP_SIZE = 40,
};

/***************** Public Prototypes ****************/
int getTimestamp(char *dest, int maxSize);

void getTimezone(char *dest, int size);

#endif
