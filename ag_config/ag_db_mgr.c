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
 Cloud & cam properties db
*/

#include "ag_db_mgr.h"


/*
Own properties list
 state_agent_on: 0/1 -- offline or connected
 connect_agent_cmd: 0/1  -- 0 - no_command, 1 - (re)connect!
 send_wd_agent_cmd: 0/1 -- 0 - no command, 1 - send request

 state_ws_on: 0/1    -- offline or connected
 connect_ws_cmd: 0/1    -- 0 - no_command, 1 - (re)connect!
 send_pong_ws_cmd: 0/1  -- 0 - no command, 1 - send pong!
 ask_4_viewers_cmd: 0/1 -- 0 - no_command, 1 - send request

 state_rw_on: 0/1    -- offline or online
 connect_rw_cmd: 0/1        -- 0 - no command, 1 - (re)connect
 disconnect_rw_cmd: 0/1     -- 0 - no command, 1 - disconnect





Cam properties list
 viewersCount: int set when WS sends the "viewersCount"  Actions: from >0 to 0 -> stop streaming other changes -> no action
 pingInterval: int set whin WS send the "pingInterval"
 ppc.streamStatus: 0/1  0 - no streaming or stop streaming. 1 - streaming required
 ppc.rapidMotionStatus: The time between each new recording in seconds. 60 - 3600
 motionStatus: Measurement and status from the camera declaring if this camera is currently recording video. -1..2
 audioStatus: Whether audio detection is turned on or off. -1..2
 recordStatus: Measurement and command from the camera declaring if this camera is currently recording video or audio.
 ppc.recordSeconds: The set maximum length of each recording 5..300 on the cam is 20..600	Can't set it on camera!
 ppc.motionSensitivity: The motion sensitivity of the camera 0..40/cam limits 0..5
 ppc.motionCountDownTime: The initial countdown time when motion recording is turned on. 5..60. Use for MD and SD - both Can't set on Cam!
 ppc.motionActivity: Current state of motion activity.0/1	-- should be sent when alert starts (1) and when it stops (0)
 ppc.audioActivity: Current state of audio activity.0/1	-- should be sent when alert starts (1) and when it stops (0)
 audioStreaming: Sets audio streaming and recording on this camera.0/1 -- Just internal thing - how to configure the stream
 videoStreaming: Sets video streaming on this camera. 0/1  - Just internal thing how to configure the stream
 supportsVideoCall: 0 and not changed!
 version: The current version of Presence running on this device. String using the format “x.x.x”
 availableBytes: The amount of memory (in bytes) available on this device
 ppc.recordFullDuration: 1 and can't be changed!
 ppc.captureImage: Measurement/command
 streamError: Value is string. Should be sent to WS in case of streaming error
 ppc.audioSensitivity: The audio sensitivity of the camera

 */
