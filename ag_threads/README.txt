Release 1.0.4

ag_threads
    at_main_thread  - Main Cam loop. Make all startup and shutdown activities.
                      Run all threads and EM process.
    at_proxy_read   -  Thread to read messages from Proxy and EM and forward it to Agent.
                       Wrapper for TCP-queue transport.
    at_proxy_rw     - Thread-manager for assync IO Agent-Proxy.
    at_proxy_write  - Thread to write to Proxy from Agent.
                      Wrapper for queue - TCP interfaces.
    at_rw_thread    - Thread(s) supporting streaming Cam->Agent->Videoserver (Wowza).
                      Initially was designed for interleaved and non-interleaved modes.
                      Finally just interleaved mode is supported. Has to be cleaned-up.
    at_ws           - WebSocket IO interface thread.
    at_wud_write    - Agent -> WUD async write thread. Wrapper for queue-TCP interface.

