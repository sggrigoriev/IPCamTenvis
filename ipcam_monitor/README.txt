Release 1.0.4

ipcam_monitor
    at_cam_alerts_reader    - Main camera monitor process function.
    at_cam_files_sender     - Camera's generated files sender (SF).
                                1. Get alert from Agent.
                                2. Put the file senf task to queue.
                                3. Send the file to the cloud.
    ipcam_monitor           - Main routine for EM process:
                                1. Catch cam events: MD/SD/file ready.
                                2. Send to Agent start/stop MD & SD.
                                3. Send to Agent files with MD/SD/Snapshots names.
                                4. Send WD to the Agent.