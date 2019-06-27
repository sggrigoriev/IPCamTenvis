Release 1.0.4

ag_config
    ag_db_mgr   - Module contains all interfaces and datatypes for properties dB.
                  This is common data & mapping for the cloud & the cam properties
                  All functions except load/unload are thread-protected!
                  Rule of use it in outer space:
                    1. Set/update values.
                    2. Make actions accordingly to dB values.
                    3. Set new values to the camera (if any) (cam_method is not NULL && changed == 1).
                    4. Save new persistently kept data (if any) (persistent == 1 && changed == 1).
                    5. Make reports to the cloud/WS (is_changes_report == 1 && change_flag == 1).
                    6. Clear flags (change_flag=changed=0;).
    ag_defaults - Contains defaults for the Tenvis Agent.
    ag_settings - Getters and setters for Camera configuration file