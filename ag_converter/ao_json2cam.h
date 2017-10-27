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
 Converts JSON-style commands from cloud to cam's language
*/

#ifndef IPCAMTENVIS_AO_JSON2CAM_H
#define IPCAMTENVIS_AO_JSON2CAM_H

/*********************************************************************
 * Converts JSON-style commands into the Camera's words
 * @param json          - input 0-terminated JSON string
 * @param cam_lingva    - output 0-terminated string with some html staf
 * @param max_size      - output buf size
 * @return              - 1 if Ok, 0 if not, If 0 the cam_lingva containg diagnistics
 */
int ao_json2cam(const char* json, char* cam_lingva, size_t max_size);

#endif /* IPCAMTENVIS_AO_JSON2CAM_H */
