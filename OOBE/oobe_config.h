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
 Created by gsg on 12/06/18.
*/

#ifndef IPCAMTENVIS_OOBE_CONFIG_H
#define IPCAMTENVIS_OOBE_CONFIG_H
/*
 * Return 1 if success
 * Loads Activation key & main cloud URL from Cam's OOBE file
 * NB! Saves Main Cloud URL info corresponding file and in CFG memory as well!
 */
int oobe_getCloudParams(const char* file_name);
/*
 * Return Activation Key or NULL if no key found
 */
const char* oobe_getActivationKey();

int oobe_load_config(const char* cfg_file_name);
/*
 * Reads data from special file. File name stored in config
 */
const char* oobe_getMainCloudURL();
const char* oobe_getMainCloudURLFileName();
const char* oobe_getAuthToken();       /* return empty string if no value or the value > max_len */
const char* oobe_getProxyDeviceID();   /* same... */

int oobe_saveAuthToken(const char* new_at);           /* Return 1 of success, return 0 if not */

int oobe_saveProxyDeviceID(const char* new_da);       /* Return 1 of success, return 0 if not */

void oobe_save_contact_url(const char* host, const char* uri, const char* port, int use_ssl);
const char* oobe_get_contact_url();


#endif /* IPCAMTENVIS_OOBE_CONFIG_H */
