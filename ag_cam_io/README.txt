Release 1.0.4

ag_cam_io
    ac_alfapro      - RTSP implementation for AlfaPro camera on cURL. cURL has to be changed to gstreamer lib.
    ac_cam          - Interface to run camera's commands.
    ac_cam_types    - Types, constants an helpers used for Cam RTSP session implementation
    ac_cloud        - Support retrieving video parameters and WS interface parameters from the cloud
    ac_http         - Local cURL wrapper. Has to be joined with libhttp sometime.
    ac_rtsp         - Video server <-> proxy <-> camera RSTP portocol support. The Agent plays as proxy for cloud
                      viewer and camera. Contains RTSP protocol support.
    ac_tcp          - TCP in/out primitives. Obsolete.
    ac_udp          - Read/write streaming functions to read from Camera and send it immediately to videoserver.
                      Name is wrong. Has to be renamed/redesigned.
    ac_video        - High-level interfaces to manage video streaming.
    ac_wowza        - Videoserver (WOWZA) RTSP protocol implementation.
    ag_digest       - Responsible for DIGEST auth. Not in use after shifting to gstreamer library.
