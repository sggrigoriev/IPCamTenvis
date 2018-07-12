#!/bin/ash
PRESTO_PATH=/root/presto

export PATH=$PRESTO_PATH/bin:$PATH
export LD_LIBRARY_PATH=$PRESTO_PATH/lib:$LD_LIBRARY_PATH

CLOUD_URL_FILE="/root/presto/bin/cloud_url"
AUTH_TOKEN_FILE="/root/presto/bin/auth_token"

WIFI_NAME="ra0"

DEVICE_TYPE="7000"

ESPAPI_URL="$CLOUD_HOST/espapi/watch"
CURL_COMMAND="curl -k --cacert /root/presto/bin/cacert.pem"
PING_COMMAND="curl 'http://127.0.0.1:8001/playsndfile?file=Ping.wav&async=1' 2>/dev/null"
BELL_COMMAND="curl 'http://127.0.0.1:8001/playsndfile?file=DoorBell.wav&async=1' 2>/dev/null"
WHOOPS_COMMAND="curl 'http://127.0.0.1:8001/playsndfile?file=Whoops.wav&async=1' 2>/dev/null"
ping_command ()
{
if [ -f $AUTH_TOKEN_FILE ];
then
	echo No Ping sound;
else
    r=$(eval $PING_COMMAND);
fi;
}

bell_command ()
{
if [ -f $AUTH_TOKEN_FILE ];
then
	echo No Ping sound;
else
    r=$(eval $BELL_COMMAND);
fi;
}


cd $PRESTO_PATH/bin;
############################ check WPA state #################################
while [ "$WPA_STATE" != "COMPLETED" ]
do
	WPA_STATE=$(wpa_cli status | grep wpa_state | awk -F "=" '{ printf("%s",$2);}')
	echo waiting WiFi connection
	if [ "$WPA_STATE" != "COMPLETED" ];
	then
		sleep 5;
	fi;
done
echo WiFi is connected;

########## check WiFi interface is on and connected ##########################
WLAN_IP=""
WLAN_IP=$(ifconfig $WIFI_NAME | grep "inet addr" | awk '{printf("%s",$2);}' | awk -F ':' '{printf("%s",$2);}')

if [ "$WLAN_IP" != "" ];
then
	echo WiFi IP: "$WLAN_IP"
else
        WLAN_IP="";
	while [ "$WLAN_IP" == "" ]
        do
		WLAN_IP=$(ifconfig $WIFI_NAME | grep "inet addr" | awk '{printf("%s",$2);}' | awk -F ':' '{printf("%s",$2);}');
		if [ "$WLAN_IP" == "" ];
		then
			echo "no connectivity";
			sleep 5;
		fi;
        done;
fi;
sleep 1;
bell_command;
###################### generate Device Id
DeviceID=$(/root/presto/bin/Proxy -d)
echo "Device ID: $DeviceID"
sleep 1

####################### check ip connectivity #################################
pingf=$(ping -c 5 8.8.8.8)
pingf=$?
echo $pingf
if [ "$pingf" != "0" ];
then
	while [ "$pingf" != "0" ]
	do
    		echo "no connectivity"
    		sleep 5
		pingf=$(ping -c 5 8.8.8.8)
	done;
else
	echo "IP connectivity is OK"
fi;

#################### check if cloud_url exists ###################
cloud_url=""
if [ -f $CLOUD_URL_FILE ];
then
	echo "Cloud URL file exists";
	cloud_url=$(eval cat $CLOUD_URL_FILE);
	echo "cloud url from file:" $cloud_url;
fi;
JSON="";
while [[ -z $cloud_url ]];
do
	JSON=$(eval $CURL_COMMAND http://127.0.0.1:8001/getalp 2>/dev/null);
	echo "<- ALP parameters" $JSON;
        cloud_url=$(echo $JSON | jq -r .cloud );
	if  [ "$cloud_url" == "" ];
	then
		sleep 1;
	fi;
done;
echo cloud_url: $cloud_url;
printf "$cloud_url" > $CLOUD_URL_FILE

####################  target URL #######################################
ping_command;
JSON="$CURL_COMMAND '$cloud_url/cloud/json/settingsServer?type=deviceio&deviceId=$DeviceID&ssl=true&timeout=30' 2>/dev/null"
echo "->" $JSON
target_url=$(eval $JSON)
echo target url: $target_url

######################## check if target URL is alive ###############################
ping_command;
deviceIOWatch="$CURL_COMMAND '$target_url/deviceio/watch' 2>/dev/null"
echo $deviceIOWatch
resultCode=$(eval $deviceIOWatch)
echo DeviceIO watcher status is $resultCode

if [ "$resultCode" = "OK" ]
then
	echo "DeviceIO is OK"
else
	while [ "$resultCode" != "OK" ]
	do
	    echo "DeviceIO is unavailable"
#	    r=$(eval $PING_COMMAND);
	    deviceIOWatch="$CURL_COMMAND '$target_url/deviceio/watch' 2>/dev/null";
	    resultCode=$(eval $deviceIOWatch);
	    sleep 5;
	done;
fi

sleep 1
ping_command;
JSON=""
api_key=""
######################## check auth token existence and request new if unavailable###############################
if [ -f $AUTH_TOKEN_FILE ];
then
	AUTH_TOKEN=$(cat $AUTH_TOKEN_FILE);
	echo Auth Token is $AUTH_TOKEN;
else
	api_key="";
	while [ "$api_key" == "" ];
	do
		JSON=$(eval $CURL_COMMAND http://127.0.0.1:8001/getalp 2>/dev/null );
		echo "<-" $JSON
		api_key=$(echo $JSON | jq -r .key );
		sleep 1;
	done;
	echo api_key is $api_key;

	JSON="";
	result=""
	while [ "$result" == "" ];
	do
		JSON="$CURL_COMMAND '$cloud_url/cloud/json/devices?deviceId=$DeviceID&deviceType=$DEVICE_TYPE&authToken=true' -H 'ACTIVATION_KEY: $api_key' -H 'Content-Type: application/json' -H 'Connection: keep-alive' --data '' 2>/dev/null";
		echo "->" $JSON;
		result=$(eval $JSON)
		sleep 1;
	done;
	echo "<-" $result;

	resultCode=$(echo $result | jq -r .resultCode );
	if [ "$resultCode" == "2" ] || [ "$resultCode" == "12" ]; then
		echo $( echo $result | jq -r .resultCodeMessage );
		r=$(eval $WHOOPS_COMMAND);
		echo "!!!!!!! WRONG TOKEN"
		rm $CLOUD_URL_FILE
		rm $AUTH_TOKEN_FILE
		cd /mnt/mtd;
		rm -f cfg/*
		tar xzvf cfg.tar.gz
		echo "!!!!REBOOTING!!!!"
		reboot;
	fi;

	if [ "$resultCode" == "" ] || [ "$resultCode" == "30" ];
	then
		watcherResultCode="";
		echo " !!!! CONNECTIVITY ISSUE !!!!!"
		while [ "$watcherResultCode" != "OK" ]
		do
			WATCHER="$CURL_COMMAND $target_url/espapi/watch 2> /dev/null";
			watcherResultCode=$(eval $WATCHER);
			if [ "$watcherResultCode" == "OK" ];
			then
				echo "!!!!!! CONNECTIVITY RESTORED !!!!!";
				break;
			fi;
			echo "sleep....";
			sleep 5;
		done;
	fi;

	if [ "$resultCode" == "0" ] ;
	then
		if [ "$( echo $result | jq -r .exist )" == "true" ];
		then
			echo "Camera $DeviceID ALREADY registered"
		else
			echo "Camera $DeviceID SUCCESSFULLY registered"
		fi;
		AUTH_TOKEN=$( echo $result | jq -r .authToken );
		if [ "$AUTH_TOKEN" != "" ];
		then
			bell_command;
			echo "Camera $DeviceID succesfully registered";
			printf "$AUTH_TOKEN" > $AUTH_TOKEN_FILE;
#			exit 0;
		fi;
	fi;
fi;

##### check if Token is OK ##############
resultCode="";
ping_command;
data="{\"proxyId\": \"$DeviceID\", \"sequenceNumber\": \"seqNo\"}"
JSON="$CURL_COMMAND '$target_url/deviceio/mljson?id=$DeviceID&ssl=true' -H 'PPCAuthorization: esp token=$AUTH_TOKEN' --data '$data' 2>/dev/null";
echo "->" $JSON;
result=$(eval $JSON)
echo "<-" $result;
if [ "$( echo $result | jq -r .status )" == "UNKNOWN" ];
then
	echo WARNING, AUTH_KEY is Obesolete, rebooting;
	r=$(eval $WHOOPS_COMMAND);
	rm $CLOUD_URL_FILE;
	rm $AUTH_TOKEN_FILE;
	cd /mnt/mtd;
	rm -f cfg/*
	tar xzvf cfg.tar.gz;
	reboot;
fi;

echo  "Now we can start Presto stuff";
./WUD &


