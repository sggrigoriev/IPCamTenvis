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
 Created by gsg on 25/09/18.
 Module contains all interfaces and datatypes for properties dB
 This is common data & mapping for the cloud & the cam properties
*/

#ifndef IPCAMTENVIS_AG_DB_MGR_H
#define IPCAMTENVIS_AG_DB_MGR_H

#include "ao_cmd_data.h"

int ag_db_load_cam_properties();
void ag_db_unload_cam_properties();
void ag_db_commit();

/* Work with bool parameters - switchers */
/*
 * 1->new_value
 */
void ag_db_set_switcher_on(const char* bool_property_name);
/*
 * 0->new_value
 */
void ag_db_set_switcher_off(const char* bool_property_name);
/*
 * new_value = 1 and value = 0
 */
int ag_db_switch_on(const char* bool_property_name);
/*
 * new_value = 0 and value = 1
 */
int ag_db_switch_off(const char* bool_property_name);
/*
 * return value
 */
int ag_db_get_switcher(const char* bool_property_name);

/*
 * Return 0 if no change; return 1 if proprrty changed
 * property_value -> new_value
 */
int ag_db_store_property(const char* property_name, const char* property_value);

/*
 * Return value
 */
const char* ag_db_get_property_value(const char* property_name);

/*
 * value <- new_value
 */
void ag_db_commit_property(const char* property_name);
/*
 * new_value -> value
 */
void ag_db_rollback_property(const char* property_name);
#endif /* IPCAMTENVIS_AG_DB_MGR_H */
