#!/bin/ash


#
#
#


source /root/presto/bin/presto.conf

export PATH=$PRESTO_PATH/bin:$PATH
export LD_LIBRARY_PATH=$PRESTO_PATH/lib:$LD_LIBRARY_PATH

#################################################################
#  internal funtions/procedures
#################################################################


#performs upgrade check
firmware_upgrade () {

UPGRADE_DIR=$(cat $WUD_CONFIG_FILE | $JQ_COMMAND FW_UPGRADE_FOLDER);
OTA_UPGRADE_FILE=$(cat $WUD_CONFIG_FILE | $JQ_COMMAND FW_UPGRADE_FILE_NAME);

echo " ****** checking for $UPGRADE_DIR/$OTA_UPGRADE_FILE  *******"
if [ -e $UPGRADE_DIR/$OTA_UPGRADE_FILE ]; 
then
	echo "******** OTA upgrade file exists, preforming upgrade ******"
	cd $UPGRADE_FOLDER
	rm -rf /tmp/presto
	tar xzf $OTA_UPGRADE_FILE -C /tmp
	cd /tmp/presto
	./make_install
	cd -
	rm -rf /tmp/presto
	rm -f $UPGRADE_DIR/$OTA_UPGRADE_FILE
	sync
else
	echo " ****** No OTA Upgrade file $UPGRADE_DIR/$OTA_UPGRADE_FILE exists, proceeding *******"
fi
}

#issue ping sound
ping_command () {
if [ -f $AUTH_TOKEN_FILE ];
then
	echo No Ping sound;
else
	r=$(eval $PING_COMMAND);
fi;
}

# qr prompt
start_qr_command () {
	r=$(eval $START_QR_COMMAND);
}

#qr success
success_qr_command () {
if [ -f $AUTH_TOKEN_FILE ];
then
	echo No qr success sound;
else
	r=$(eval $QR_SUCCESS_COMMAND);
fi;
}

#issue bell sound
bell_command () {
if [ -f $AUTH_TOKEN_FILE ];
then
	echo No bell sound;
else
	r=$(eval $BELL_COMMAND);
fi;
}

#issue bell sound
bell_command () {
if [ -f $AUTH_TOKEN_FILE ];
then
	echo No bell sound;
else
	r=$(eval $BELL_COMMAND);
fi;
}

#issue bell sound
whoops_command () {
if [ -f $AUTH_TOKEN_FILE ];
then
	echo No Whoops sound;
else
	r=$(eval $WHOOPS_COMMAND);
fi;
}

# reset and rebooot device
reset_and_reboot () {

	killall -9 sound.sh
	sleep 2;
	killall -9 sound.sh
	whoops_command;
	sleep 2;
	whoops_command;
	sleep 2;
	rm $CLOUD_URL_FILE;
	rm $AUTH_TOKEN_FILE;
	cp $PRESTO_PATH/bin/alp_tw.ini /mnt/mtd
	cd /mnt/mtd;
	rm -f cfg/*
	tar xzvf cfg.tar.gz;
	sync;
	$REBOOT_COMMAND;
}

# check WPA state
check_wpa_state () {
ntries=0;
v_connected=1
echo "Checking WPA state......"
while [ "$WPA_STATE" != "COMPLETED" ]
do
	WPA_STATE=$(wpa_cli status | grep wpa_state | awk -F "=" '{ printf("%s",$2);}')
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


# check WLAN interface IP
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

# check IP connectivity using given ip address
check_ip_connectivity () {
ntries=0;
pingf="";
v_connected=1
while [ "$pingf" != "0" ];
do
	r=$(ping -c $IP_PING_TRIES $IP_PING_ADDRESS)
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
		v_connected=0;
    fi;
done;
return "$v_connected";
}

#
# this function permanently checks ip accessibility and in case of any connectivity issue, reboots camera
#
permanent_check_ip_connectivity () {
ntries=0;
ret=0;
while [ "0" == "0" ];
do
	r=$(ping -c $IP_PING_TRIES $IP_PING_ADDRESS)
	ret=$?;
	if [ "$ret" == "0" ];
	then
#		echo "IP Connectivity is OK.";
		ntries=0;
		sleep 5;
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
		sleep 5;
    fi;
done;
}



# check CGI for the cloud/key attributes. every cycle repeat start_qr.wav

read_cloud_parameters ()
{
cloud_url="";
api_key="";
ssid="";
while [ "1" == "1" ];
do
    JSON=$(eval $CURL_COMMAND $CGI_URL 2>/dev/null);
    sleep 1;
    if [ "$JSON" == "" ];
    then
            continue;
    fi;
    echo "!!!!! <- ALP parameters" $JSON;
    cloud_url=$(echo $JSON | $JQ_COMMAND cloud 2>/dev/null );
    api_key=$(echo $JSON | $JQ_COMMAND key 2>/dev/null);
    ssid=$(echo $JSON | $JQ_COMMAND ssid 2>/dev/null);
    if  [ "$cloud_url" == "" ] || [ "$api_key" == "" ] || [ "$ssid" == "" ];
    then
            echo "Playing QR prompt sound";
            start_qr_command;
            sleep 7;
    else
            success_qr_command;
            break;
            sleep 2;
    fi;
done;
}

#############################################################################
#
#############################################################################

sleep 5;

cd $PRESTO_PATH/bin;
killall -9 Proxy WUD Tenvis

# check and do fw aupgrade if  found
firmware_upgrade;

# check  cloud parameters (key/cloud) if not ready prompt qr code sound every 5 seconds
read_cloud_parameters;
# ++++++++++++ check WiFi attach & WLAN interface readiness  ++++++++++++
./sound.sh Ping.wav &

check_ip_connectivity;
v_ip=$?
if [ "$v_ip" != "0" ];
then
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
# ++++++++++++ check IP Connectivity prior to dealing with the cloud ++++++++++++
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
	r=$(eval killall -9 sound.sh);
	echo "Camera is not connected to the internet. Restaring"
	$REBOOT_COMMAND;
fi;


# ++++++++++++ generate Device Id ++++++++++++
DeviceID=$($PRESTO_PATH/bin/Proxy -d)
echo "Device ID: $DeviceID"

sleep 2
#################### check if cloud_url exists ###################
cloud_url=""
if [ -f $CLOUD_URL_FILE ];
then
    #file exists
	echo "Cloud URL file exists";
	cloud_url=$(eval cat $CLOUD_URL_FILE);
	echo "cloud url from file:" $cloud_url;
else
	#getting from CGI
	JSON="";
	while [[ -z $cloud_url ]];
	do
		JSON=$(eval $CURL_COMMAND $CGI_URL 2>/dev/null);
		echo "<- ALP parameters" $JSON;
		cloud_url=$(echo $JSON | $JQ_COMMAND cloud );
		if  [ "$cloud_url" == "" ];
		then
			sleep 1;
		fi;
	done;
fi;
echo cloud_url: $cloud_url;


######################## get & check if target URL is alive ###############################
resultCode="";
JSON="";
ntries=0;
while [ "$resultCode" != "OK" ]
do
	JSON="$CURL_COMMAND '$cloud_url/cloud/json/settingsServer?type=deviceio&deviceId=$DeviceID&ssl=true&timeout=30' 2>/dev/null"
	echo " target URL ->" $JSON
	target_url=$(eval $JSON)
	echo "target URL <-" "$JSON"
	echo target url: "$target_url"
	deviceIOWatch="$CURL_COMMAND '$target_url/$DEVICEIO_URL' 2>/dev/null";
	resultCode=$(eval $deviceIOWatch);
	if [ "$resultCode" != "OK" ];
	then
		ntries=$(expr $ntries + 1);
		if [ "$ntries" -gt "$IP_CONNECTIVITY_MAX_TRIES" ];
		then
			echo "Max number if tries exceeded rebooting";
			$REBOOT_COMMAND;
		fi;
		echo "<-" $resultCode;
		echo "<- DeviceIO is unavailable : $resultCode";
		sleep 5;
	else
		echo "DeviceIO is OK";
		break;
	fi;
done;

sleep 1
JSON=""
api_key=""
######################## check auth token existence and request new if unavailable ###############################
if [ -f $AUTH_TOKEN_FILE ];
then
	AUTH_TOKEN=$(cat $AUTH_TOKEN_FILE);
	echo Auth Token is $AUTH_TOKEN;
else
    # get temporary token from CGI
	api_key="";
	while [ "$api_key" == "" ];
	do
		JSON=$(eval $CURL_COMMAND $CGI_URL 2>/dev/null );
		echo "<-" $JSON
		api_key=$(echo $JSON | $JQ_COMMAND key );
		sleep 1;
	done;
	echo api_key is $api_key;

	JSON="";
	result=""
	v_connectivity="";
	# get target token from cloud using temp token
	while [ "$result" == "" ];
	do
		JSON="$CURL_COMMAND '$cloud_url/cloud/json/devices?deviceId=$DeviceID&deviceType=$DEVICE_TYPE&authToken=true' -H 'ACTIVATION_KEY: $api_key' -H 'Content-Type: application/json' -H 'Connection: keep-alive' --data '' 2>/dev/null";
		echo "->" $JSON;
		result=$(eval $JSON)
		echo "<-" register device answer: $result;
		if [ "$result" != "" ];
		then
			resultCode=$(echo $result | $JQ_COMMAND resultCode );
		else
			resultCode="";
		fi;
		if [ "$resultCode" == "" ] || [ "$resultCode" == "30" ] || [ "$result" == "" ];
		then
			# connectivity issue
			v_watcherResultCode="";
			echo " !!!! CONNECTIVITY ISSUE !!!!!"
			while [ "$v_watcherResultCode" != "OK" ]
			do
				WATCHER="$CURL_COMMAND $target_url/$ESPAPI_URL 2> /dev/null";
				v_watcherResultCode=$(eval $WATCHER);
				if [ "$v_watcherResultCode" == "OK" ];
				then
					echo "!!!!!! CONNECTIVITY RESTORED !!!!!";
					v_connectivity="OK";
					break;
				else
					echo "<- ESPAPI is unavailable:" $v_watcherResultCode;
					sleep 5;
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
	echo "<-" $result;
    # check result code
    resultCode=""
	resultCode=$(echo $result | $JQ_COMMAND resultCode );
	if [ "$resultCode" == "2" ] || [ "$resultCode" == "12" ];
	then
		# this is wrong token, initiating reset
		echo $( echo $result | $JQ_COMMAND resultCodeMessage );
		echo "<<<<<<<<<<<<<<<<< WRONG TOKEN >>>>>>>>>>>>>>>>>>>>>>>"
		reset_and_reboot;
	fi;
	if [ "$resultCode" == "22" ];
	then
		# this is wrong token, initiating reset
		echo $( echo $result | $JQ_COMMAND resultCodeMessage );
		echo "<<<<<<<<<<<<<<< Device under another location. Reset and reboot >>>>>>>>>>>>>>>>"
		reset_and_reboot;
	fi;

	if [ "$resultCode" == "0" ] ;
	then
		if [ "$( echo $result | $JQ_COMMAND exist )" == "true" ];
		then
			echo "Camera $DeviceID ALREADY registered"
		else
			echo "Camera $DeviceID SUCCESSFULLY registered"
		fi;
		AUTH_TOKEN=$( echo $result | $JQ_COMMAND authToken );
		if [ "$AUTH_TOKEN" != "" ];
		then
			echo "Camera $DeviceID succesfully registered";
		fi;
	fi;
fi;

##### check if Token is OK ##############
resultCode="";
data="{\"proxyId\": \"$DeviceID\", \"sequenceNumber\": \"seqNo\"}"
JSON="$CURL_COMMAND '$target_url/deviceio/mljson?id=$DeviceID&ssl=true' -H 'PPCAuthorization: esp token=$AUTH_TOKEN' --data '$data' 2>/dev/null";
echo "->" $JSON;
result=$(eval $JSON)
echo "<-" $result;
if [ "$( echo $result | $JQ_COMMAND status )" == "UNKNOWN" ];
then
	echo "<<<<<<<<<<<<<<<<<<< WARNING, AUTH_KEY is Obsolete, rebooting >>>>>>>>>>>>>>>>>>>>>>";
	reset_and_reboot;
fi;
if [ "$( echo $result | $JQ_COMMAND status )" == "UNAUTHORIZED" ];
then
	echo "<<<<<<<<<<<<<<<<<<<<< WARNING, AUTH_KEY is bad, rebooting >>>>>>>>>>>>>>>>>>>>>>>>>>>";
	reset_and_reboot;
fi;

r=$(eval killall -9 sound.sh);
sleep 2;
bell_command;
printf "$AUTH_TOKEN" > $AUTH_TOKEN_FILE;
printf "$cloud_url" > $CLOUD_URL_FILE
echo  "Now we can start Presto stuff";
./WUD &
# start ip check in a background
permanent_check_ip_connectivity;

