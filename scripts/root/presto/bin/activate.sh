#!/bin/ash


#
#
#

export LD_LIBRARY_PATH=/root/presto/lib:/mnt/mtd/lib:$LD_LIBRARY_PATH
source /root/presto/bin/presto.conf

export PATH=$PRESTO_PATH/bin:$PATH
export LD_LIBRARY_PATH=$PRESTO_PATH/lib:$LD_LIBRARY_PATH
#/komod/mt7601.sh load


#$(eval rm /root/presto/log/* 2>/dev/null)
#################################################################
#  internal funtions/procedures
#################################################################

check_and_write_ipc_pid ()
{
  while true;
  do
     ipc_pid=$(ps | grep [i]pc-hi3516 | awk '{print $1}');
     if [ "$ipc_pid" != "" ];
     then
            printf "$ipc_pid" > $PRESTO_HOME/bin/ipc.pid;
            return 0;
    else
            sleep 2;
    fi;
  done;
}


check_upgrade_status ()
{
  target_url=$3;
  auth_token=$2;
  device_id=$1;

  current_release=$(cat $PRESTO_PATH/firmware_release);
  if [ -f $PRESTO_PATH/upgrade_status ];
  then
        source $PRESTO_PATH/upgrade_status;
        if [ "$INSTALL_STATUS" != "0" ];
        then
                echo "WARNING: unsuccesfull upgrade";
                data="{\"proxyId\": \"$device_id\", \"sequenceNumber\": \"1\",\"alerts\"   : [{\"deviceId\": \"$device_id\",\"alertId\": \"ALERT_ID2\",  \"alertType\": \"error\", \"paramsMap\": { \"error\": \"Upgrade error: $MESSAGE\" }}]}"
                JSON="$CURL_COMMAND '$target_url/deviceio/mljson?id=$device_id&ssl=true' -H 'PPCAuthorization: esp token=$auth_token' --data '$data' 2>/dev/null";
                echo "INFO: -> $JSON";
                result=$(eval $JSON)
                echo "INFO: <- $result";
                data="{\"proxyId\": \"$device_id\", \"sequenceNumber\": \"1\", \"measures\": [{\"deviceId\": \"$device_id\",\"params\": [{\"name\": \"firmware\", \"value\": \"$current_release\"},{\"name\": \"firmwareUpdateStatus\", \"value\": \"3\"}]}]}"
                # here we have to notify cloud regarding upgrade

        else
                echo "INFO: Previous upgrade was succesfull";
                data="{\"proxyId\": \"$device_id\", \"sequenceNumber\": \"1\", \"measures\": [{\"deviceId\": \"$device_id\",\"params\": [{\"name\": \"firmware\", \"value\": \"$current_release\"},{\"name\": \"firmwareUpdateStatus\", \"value\": \"0\"}]}]}"

        fi;
  else
        #assuming that previous upgrade was succesfull and we deleted file
        INSTALL_STATUS=0
        data="{\"proxyId\": \"$device_id\", \"sequenceNumber\": \"1\", \"measures\": [{\"deviceId\": \"$device_id\",\"params\": [{\"name\": \"firmware\", \"value\": \"$current_release\"},{\"name\": \"firmwareUpdateStatus\", \"value\": \"0\"}]}]}"
  fi;
  JSON="$CURL_COMMAND '$target_url/deviceio/mljson?id=$device_id&ssl=true' -H 'PPCAuthorization: esp token=$auth_token' --data '$data' 2>/dev/null";
  echo "INFO: -> $JSON";
  result=$(eval $JSON)
  echo "INFO: <- $result";
  $(rm $PRESTO_PATH/upgrade_status 2>/dev/null)
}

#################################################################
# qr prompt
#################################################################
start_qr_command () {
        r=$(eval $START_QR_COMMAND);
}

#################################################################
#qr success
#################################################################
success_qr_command () {
if [ -f $AUTH_TOKEN_FILE ];
then
        echo "INFO: No qr success sound";
else
        r=$(eval $QR_SUCCESS_COMMAND);
fi;
}

#################################################################
#bell sound
#################################################################
bell_command () {
if [ -f $AUTH_TOKEN_FILE ];
then
        echo "INFO: No bell sound";
else
        r=$(eval $BELL_COMMAND);
fi;
}

#################################################################
#whoops sound
#################################################################
whoops_command () {
if [ -f $AUTH_TOKEN_FILE ];
then
        echo "INFO: No Whoops sound";
else
        r=$(eval $WHOOPS_COMMAND);
fi;
}

#################################################################
# reset and rebooot device
#################################################################
reset_and_reboot () {

        $($killall -9 sound.sh 2>/dev/null)
        sleep 2;
        $(killall -9 sound.sh 2>/dev/null)
        rm $CLOUD_URL_FILE;
        rm $AUTH_TOKEN_FILE;
        whoops_command;
        sleep 2;
        cp $PRESTO_PATH/bin/alp_tw.ini /mnt/mtd
        cd /mnt/mtd;
        rm -f cfg/*
        tar xzvf cfg.tar.gz;
        sync;
        $REBOOT_COMMAND;
}

#################################################################
# check WPA state
#################################################################
check_wpa_state () {
ntries=0;
v_connected=1
echo "INFO: Checking WiFi WPA state......";
while [ "$WPA_STATE" != "COMPLETED" ]
do
        WPA_STATE=$(wpa_cli status | grep wpa_state | awk -F "=" '{printf("%s",$2);}')
        if [ "$WPA_STATE" != "COMPLETED" ];
        then
                v_connected=1;
                ntries=$(expr $ntries + 1);
                if [ "$ntries" -gt "$IP_CONNECTIVITY_MAX_TRIES" ];
                then
                        echo "WARNING: Maximum number of tries exceeded, rebooting.";
                        reset_and_reboot;
                        break;
                fi;
                echo "WARNING: WPA is not connected. Trying $ntries of total $IP_CONNECTIVITY_MAX_TRIES tries";
                sleep 5;
        else
                v_connected=0;
        fi;
done
return "$v_connected";
}


#################################################################
# check WLAN interface IP
#################################################################
check_wlan_interface () {
WLAN_IP=""
ntries=0;
v_connected=1
while [ "$WLAN_IP" == "" ];
do
        WLAN_IP=$(ifconfig $WIFI_INTERFACE | grep "inet addr" | awk '{printf("%s",$2);}' | awk -F ':' '{printf("%s",$2);}');
        if [ "$WLAN_IP" == "" ];
        then
                v_connected=1;
                ntries=$(expr $ntries + 1);
                if [ "$ntries" -gt "$IP_CONNECTIVITY_MAX_TRIES" ];
                then
                        echo "WARNING: Maximum number of tries exceeded, rebooting.";
                        whoops_command;
                        sync;
                        $REBOOT_COMMAND;
                        break;
                fi;
                echo "WARNING: WLAN IP is not set ($WLAN_IP). Try $ntries of total $IP_CONNECTIVITY_MAX_TRIES tries";
                sleep 5;
        else
                v_connected=0;
        fi;
done;
return "$v_connected";
}

#################################################################
# check IP connectivity using given ip address
#################################################################
check_ip_connectivity () {
ntries=0;
pingf="";
v_connected=1
while [ "$pingf" != "0" ];
do
        r=$(ping -c $IP_PING_TRIES $IP_PING_ADDRESS 2>/dev/null)
        pingf=$?;
        if [ "$pingf" != "0" ];
        then
                v_connected=1;
                ntries=$(expr $ntries + 1);
                if [ "$ntries" -gt  "$IP_CONNECTIVITY_MAX_TRIES" ];
                then
                        echo "WARNING: Maximum number of tries exceeded, rebooting.";
                        sync;
                        $REBOOT_COMMAND;
                        break;
                fi;
                echo "WARNING: NO IP connectivity. Try $ntries of total $IP_CONNECTIVITY_MAX_TRIES tries";
                sleep 2;
    else
                echo "IP Connectivity OK.";
                v_connected=0;
    fi;
done;
return "$v_connected";
}

#################################################################
# this function permanently checks ip accessibility and in case of any connectivity issue, reboots camera
#################################################################
TSTMP01="01AM";
TSTMP02="01PM";
permanent_check_ip_connectivity () {
ntries=0;
ret=0;
while sleep 30;
do
# 1 reboot every 12-00 - now made in Agent main threag
#        time=$(date +"%I%p");
#        if [ "$time"  == "$TSTMP01" ] || [ "$time"  == "$TSTMP02" ];
#        then
#                message="INFO: Time to reboot, but let's check we're not already rebooted...";
#                echo $message;
#                echo $message > /dev/console;
#                if [ ! -f $PRESTO_HOME/.reboot ];
#                then
#                        message="INFO: Rebooting.";
#                        echo $message;
#                        echo $message > /dev/console;
#                        echo "" > $PRESTO_HOME/.reboot
#                        sync;
#                        reboot;
#                fi;
#        else
#                # this is non 01AM/PM, so removing lockfile
#                if [ -f $PRESTO_HOME/.reboot ];
#                then
#                        $(rm $PRESTO_HOME/.reboot 2>/dev/null);
#                fi;
#                # echo $message;
#                # echo $message > /dev/console
#        fi;

# 2 perform cloud URL check
        if [ "$time" == "11PM" ];
        then
                message="INFO: Time to send fw version update";
                echo $message;
                echo $message > /dev/console
                get_cloud_url;
                check_upgrade_status;

#       else
#               message="INFO: Not a time to reboot";
#               echo $message;
#               echo $message > /dev/console
        fi;
        r=$(ping -c $IP_PING_TRIES $IP_PING_ADDRESS)
        ret=$?;
        if [ "$ret" == "0" ];
        then
#               echo "IP Connectivity is OK.";
                ntries=0;
                continue;
        else
                 # some connectivity issue
                ntries=$(expr $ntries + 1);
                if [ "$ntries" -gt  "$IP_CONNECTIVITY_MAX_TRIES" ];
                then
                        echo "WARNING: Maximum number of tries exceeded, rebooting.";
                        sync;
                        $REBOOT_COMMAND;
                        break;
                fi;
                echo "WARNING: NO IP connectivity. Try $ntries of total $IP_CONNECTIVITY_MAX_TRIES tries";
    fi;
done;
}

#################################################################
# check CGI for the cloud/key attributes. every cycle repeat start_qr.wav
#################################################################
read_cloud_parameters ()
{
cloud_url="";
api_key="";
ssid="";
while true;
do
    JSON=$(eval $CURL_COMMAND $CGI_URL 2>/dev/null);
    sleep 1;
    if [ "$JSON" == "" ] || [ "$JSON" == "{}" ];
    then
            echo "INFO: Playing QR prompt sound";
            start_qr_command;
            sleep 7;
            continue;
    fi;
    echo "INFO: <- ALP parameters: $JSON";
    cloud_url=$(echo $JSON | $JQ_COMMAND cloud 2>/dev/null );
    api_key=$(echo $JSON | $JQ_COMMAND key 2>/dev/null);
    ssid=$(echo $JSON | $JQ_COMMAND ssid 2>/dev/null);
    if  [ "$cloud_url" == "" ] || [ "$api_key" == "" ] || [ "$ssid" == "" ];
    then
            echo "INFO: Playing QR prompt sound";
            start_qr_command;
            sleep 7;
    else
            success_qr_command;
            $PRESTO_PATH/bin/sound.sh Ping.wav &
            break;
    fi;
done;
}

#################################################################
# get cloud url file and check it accessibility
#################################################################
get_cloud_url () {
#################### check if cloud_url exists ###################
cloud_url=""
if [ -f $CLOUD_URL_FILE ];
then
    #file exists
        echo "INFO: Cloud URL file exists";
        cloud_url=$(eval cat $CLOUD_URL_FILE);
        echo "INFO: cloud url from file: $cloud_url";
else
        #getting from CGI
        JSON="";
        while [[ -z $cloud_url ]];
        do
                JSON=$(eval $CURL_COMMAND $CGI_URL 2>/dev/null);
                echo "INFO: <- ALP parameters $JSON";
                cloud_url=$(echo $JSON | $JQ_COMMAND cloud );
                if  [ "$cloud_url" == "" ];
                then
                        sleep 1;
                fi;
        done;
fi;
if [ ! -f $AUTH_TOKEN_FILE ];
then
        return 0;
fi;

echo "INFO: cloud_url: $cloud_url";
echo "INFO: Checking DeviceIO URL... ";
resultCode="";
JSON="";
ntries=0;
while [ "$resultCode" != "OK" ]
do
        JSON="$CURL_COMMAND '$cloud_url/cloud/json/settingsServer?type=deviceio&deviceId=$DeviceID&ssl=true&timeout=30' 2>/dev/null"
        echo "INFO: target URL -> $JSON";
        target_url=$(eval $JSON)
        echo "INFO: target URL <- \"$target_url\"";

        deviceIOWatch="$CURL_COMMAND '$target_url/$DEVICEIO_URL' 2>/dev/null";
        echo "INFO: -> $deviceIOWatch";
        resultCode=$(eval $deviceIOWatch);
        echo "INFO: <- $resultCode";
        if [ "$resultCode" != "OK" ];
        then
                echo "INFO: <- DeviceIO is unavailable : $resultCode";
                ntries=$(expr $ntries + 1);
                if [ "$ntries" -gt "$IP_CONNECTIVITY_MAX_TRIES" ];
                then
                        echo "WARNING: Maximum number of tries checking DeviceIO exceeded. Reboot.";
                        $REBOOT_COMMAND;
                fi;
                sleep 3;
        else
                echo "INFO: DeviceIO is OK";
                break;
        fi;
done;
}


#############################################################################
#############################################################################
#############################################################################
#   MAIN body
#############################################################################
#############################################################################
#############################################################################

cd $PRESTO_PATH/bin;
$(killall -9 Proxy WUD Tenvis Monitor 2>/dev/null)

# check  cloud parameters (key/cloud) if not ready prompt qr code sound every 5 seconds
if [ ! -f $AUTH_TOKEN_FILE ];
then
    echo "INFO: Camera is brand new or after reset.";
    read_cloud_parameters;
fi;

# obsolete since 20182014 update check_and_write_ipc_pid;
# ++++++++++++ check WiFi attach & WLAN interface readiness  ++++++++++++

message="INFO: Checking ip connectivity";
echo $message;
check_ip_connectivity;
v_ip=$?
if [ "$v_ip" != "0" ];
then
        message="INFO: Checking WiFi wpa state";
        echo $message;
        check_wpa_state;
        v_vpa=$?
        if [ "$v_vpa" == "0" ];
        then
                echo "INFO: WPA state is OK";
        else
                echo "WARNING: WPA is not connected";
        fi;
        check_wlan_interface;
        v_wlan=$?;
        if [ "$v_wlan" == "0" ];
        then
                echo "INFO: WLAN state is OK";
        else
                echo "WARNING: WLAN is not connected";
        fi;
fi;

##### check IP Connectivity before interacting with the cloud
message="INFO: Checking ip connectivity";
echo $message;
check_ip_connectivity;
v_ip=$?
if [ "$v_ip" == "0" ];
then
        echo "INFO: IP connectivity  state is OK";
else
        echo "WARNING: IP connectivity is bad";
fi;

if [ "$v_ip" == "1" ] || [ "$v_wpa" == "1" ] || [ "$v_wlan" == "1" ];
then
        r=$(eval killall -9 sound.sh 2>/dev/null);
        echo "INFO: Camera is not connected to the internet. Restaring."
        $REBOOT_COMMAND;
fi;

# ++++++++++++ generate Device Id ++++++++++++
# this is because untill this time there is no ra0 interface
DeviceID=$($PRESTO_PATH/bin/Proxy -d)
echo "INFO: Device ID: $DeviceID"
#################### get and check cloud_url ###################
get_cloud_url;
echo "INFO: cloud_url: $cloud_url";
sleep 1
JSON=""
api_key=""
######################## check auth token existence and request new if unavailable ###############################
if [ -f $AUTH_TOKEN_FILE ];
then
        AUTH_TOKEN=$(cat $AUTH_TOKEN_FILE);
        echo "INFO: Auth Token is '$AUTH_TOKEN'";
else
        # get temporary token from CGI
        api_key="";
        while [ "$api_key" == "" ];
        do
                JSON=$(eval $CURL_COMMAND $CGI_URL 2>/dev/null );
                echo "INFO: <- $JSON";
                api_key=$(echo $JSON | $JQ_COMMAND key );
                sleep 1;
        done;
        echo "INFO: api_key is \"$api_key\"";

        JSON="";
        result=""
        v_connectivity="";
        # get target token from cloud using temp token
        while [ "$result" == "" ];
        do
                JSON="$CURL_COMMAND '$cloud_url/cloud/json/devices?deviceId=$DeviceID&deviceType=$DEVICE_TYPE&authToken=true' -H 'ACTIVATION_KEY: $api_key' -H 'Content-Type: application/json' -H 'Connection: keep-alive' --data '' 2>/dev/null";
                echo "INFO: -> $JSON";
                result=$(eval $JSON)
                echo "INFO: <- register device answer: \"$result\"";
                if [ "$result" != "" ];
                then
                        resultCode=$(echo $result | $JQ_COMMAND resultCode );
                else
                        resultCode="";
                fi;
                if [ "$resultCode" == "30" ] || [ "$result" == "" ];
                then
                        # connectivity issue
                        v_watcherResultCode="";
                        echo "WARNING: ESPAPI CONNECTIVITY ISSUE!"
                        while [ "$v_watcherResultCode" != "OK" ]
                        do
                                WATCHER="$CURL_COMMAND $target_url/$ESPAPI_URL 2> /dev/null";
                                v_watcherResultCode=$(eval $WATCHER);
                                if [ "$v_watcherResultCode" == "OK" ];
                                then
                                        echo "INFO: CONNECTIVITY RESTORED.";
                                        v_connectivity="OK";
                                        break;
                                else
                                        echo "INFO: <- ESPAPI is unavailable: \"$v_watcherResultCode\"";
                                        sleep 2;
                                fi;
                        done;
                        # repeat getting token after connectivity restoration
                        if [ "$v_connectivity" == "OK" ];
                        then
                                continue;
                        fi;
                else
                        sleep 1;
                fi;
        done;
    # check result code
    resultCode=""
        resultCode=$(echo $result | $JQ_COMMAND resultCode );
        if [ "$resultCode" == "2" ] || [ "$resultCode" == "12" ];
        then
                # this is wrong token, initiating reset
                echo $( echo $result | $JQ_COMMAND resultCodeMessage );
                echo "ERROR:  WRONG TOKEN. Reset and reboot.";
                reset_and_reboot;
        fi;
        if [ "$resultCode" == "22" ];
        then
                # this is wrong token, initiating reset
                echo $( echo $result | $JQ_COMMAND resultCodeMessage );
                echo "ERROR: Device is under another location. Reset and reboot."
                reset_and_reboot;
        fi;

        if [ "$resultCode" == "0" ] ;
        then
                if [ "$( echo $result | $JQ_COMMAND exist )" == "true" ];
                then
                        echo "INFO: Camera $DeviceID was registered before.";
                else
                        echo "INFO: Camera $DeviceID SUCCESSFULLY registered first time."
                fi;
                AUTH_TOKEN=$( echo $result | $JQ_COMMAND authToken );
                if [ "$AUTH_TOKEN" != "" ];
                then
                        echo "INFO: Camera $DeviceID successfully registered.";
                else
                        echo "ERROR: AUTH_TOKEN suddenly is empty. Reset and reboot."
                        r=$(eval killall -9 sound.sh 2>/dev/null);
                        sleep 2;
                        reset_and_reboot;
                fi;
                r=$(eval killall -9 sound.sh 2>/dev/null);
                sleep 2;
                bell_command;
                printf "$AUTH_TOKEN" > $AUTH_TOKEN_FILE;
                printf "$cloud_url" > $CLOUD_URL_FILE
        fi;
fi;

##### check if Token is OK ##############
resultCode="";
data="{\"proxyId\": \"$DeviceID\", \"sequenceNumber\": \"seqNo\"}"
JSON="$CURL_COMMAND '$target_url/deviceio/mljson?id=$DeviceID&ssl=true' -H 'PPCAuthorization: esp token=$AUTH_TOKEN' --data '$data' 2>/dev/null";
echo "INFO: -> $JSON";
result=$(eval $JSON)
echo "INFO: <- \"$result\"";
if [ "$( echo $result | $JQ_COMMAND status )" == "UNKNOWN" ];
then
        echo "ERROR: AUTH_KEY $AUTH_TOKEN is Obsolete. Reset and reboot.";
        rm $AUTH_TOKEN_FILE;
        reset_and_reboot;
fi;
if [ "$( echo $result | $JQ_COMMAND status )" == "UNAUTHORIZED" ];
then
        echo "ERROR: AUTH_KEY is bad. Reset and reboot.";
        reset_and_reboot;
fi;

# here we checking possible upgrade status if ${PRESTO_HOME/upgrade_status exists
check_upgrade_status $DeviceID $AUTH_TOKEN $target_url;

echo  "INFO: Starting Presto software Stack.";
$PRESTO_PATH/bin/WUD &
# start ip check in a background
permanent_check_ip_connectivity;
