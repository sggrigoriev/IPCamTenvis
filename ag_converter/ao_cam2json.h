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
 Created by gsg on 25/10/17.
 Concerts Cam lingva to cloud JSON
*/

#ifndef IPCAMTENVIS_AO_CAM2JSON_H
#define IPCAMTENVIS_AO_CAM2JSON_H
/********************************************************************
 * Converts cam lingva to cloud JSON contructions
 * @param cam_lingva    - input 0-terminated string with cam output
 * @param json          - output 0-terminated string with converted to JSON data
 * @param max_size      - json buf max size
 * @return              - 1 if OK, 0 if not, If 0 the json contains the diagniostics
 */

int ao_cam2json(const char* cam_lingva, char* json, size_t max_size);

#endif /* IPCAMTENVIS_AO_CAM2JSON_H */
