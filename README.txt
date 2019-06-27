Release 1.0.4

1. CMakeLists.txt defines.
    Environment variables:
        PLATFORM =
            "ubuntu"
            "hisilicon"
        LD_LIBRARY_FILES =
            List of paths to system libraries (Linux standard env variable)
            Set differently for each platform
        SDK_PATH =
            Full path to the Proxy common lib
        NO_GST_PLAGINS = - IPCamTenvis setting
            1 - add gstreamer plugins (hisilicon build)
            0 - ignore gstreamer plugins (ubuntu build)
        TOOLCHAIN =
            Path to the coross-compiler support libraries for hisilicon
            Valid for PLATFORM = "hisilicon"
        SYSROOT =
            Path to the sysroot for hisilicon envirinment on ubuntu
            Valid for PLATFORM = "hisilicon"

    CMake defines:
        GIT_COMMI
        GIT_BRANCH
        GIT_URL
        BUILD_DATE
        UNCOMMITED_CHANGES  - all these are used to print into IPCamTenvis log the current build info

        NOPOLL_TRACE        - Enabling nopoll ()Web Socket connection) logging to stdout. Used for debugging purposes.
        CURL_TRACE          - Switches on CURL trace for AlfaPro RTSP session
        RW_CYCLES=999       - Will stop RW thread after RW_CYCLES iterations. Used for stress-tests.

        __LINUX__           - Needed for IPCam external library "ipcam"
        EXTERN=             - Needed for IPCam external library "ipcam"

        GST_EXT             - Defined to avoid GST plugins use. Currently used just for ubuntu.
                              Better to make this for hisilicon as well to reduse the program size.

2. IPCamTenvis configuration file
    AGENT_PROCESS_NAME      - Printable process name to use in logging
    LOG_NAME                - log file name with path
    LOG_REC_AMT             - log file records amount until the rewrite
    LOG_LEVEL               - logging level. Possible values are from high to low:
        "ERROR", "INFO", "WARNING", "DEBUG"
    QUEUES_REC_AMT          - max amount of records stored in a queue. If the amount exceeds, the oldest will be deleted
    PROXY_PORT              - TCP port to communicate with Proxy
    WUD_PORT                - TCP port to communicate with WUD
    WATCHDOG_TO_SEC         - period to send watchdog to WUD (NB! should be less than AGENT_WD_TIMEOUT_SEC)
    DEVICE_TYPE             - camera device type defined in the cloud. Currently 7000

    IPCAM_IP                - "127.0.0.1" Another values could be used if Agent runs separately from ghe Camera for debugging.
    IPCAM_PORT              - 8001 (see cam documentation)
    IPCAM_POSTFIX           - "0" (see cam documentation)
    IPCAM_CHANNEL           - "0" for high res (works owful) "1" low res - use this one. It is work. All complains please address to cam supplier.
    IPCAM_MODE              - "av" means audio + video. Other modes yuo could find in cam documentation.
    IPCAM_LOGIN             - "admin"
    IPCAM_PASSWORD          - "admin" both could be changed via camera CGI inerface but after reset will be back th these values.
    IPCAM_PROTOCOL          - "RTSP" Corrently only this protocol supported by Agent
    INTERLEAVED_MODE        - 1 Do not change this non-interleaved mode does not support anymore because of bad streaming quaity.
    STREAMING_BUFFER_SIZE   - Buffer size for streaming transition. Current value is 128000. Shouild be enlarged in case of bad connection.

    SET_SSL_FOR_URL_REQUEST - 0 use http, 1 - use https. 0 used in debugging purposes only to have network trace
    CURLOPT_CAINFO          - cURL specific
    CURLOPT_SSL_VERIFYPEER  - cURL specific

3. activate.sh description
    WUD, Proxy, Tenvis are run from the startup sctipt activate.sh. The steps are:
        Check  cloud parameters (key/cloud) if not ready prompt qr code sound every 5 seconds
        Ping given IP address (8.8.8.8 currently). If no connection after some tries: reboot device.
        In the loop:
            Check if WiFi available and connected
        Check IP Connectivity prior to dealing with the cloud. Reboot if no connection.
        Generate Device Id and save it to the "one_string_file"
        Get Main CLoud URL from the one_string_file "cloud_url" or get it from defaults and write down to "cloud_url" file
        Get Contact URL and check the connectivity to it. Reboot if no connection.
        Check auth token existence and request new if unavailable
        Start WUD
        Continue check connectivity once in period.


