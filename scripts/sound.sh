#!/bin/ash
PRESTO_PATH=/root/presto

export PATH=$PRESTO_PATH/bin:$PATH
export LD_LIBRARY_PATH=$PRESTO_PATH/lib:$LD_LIBRARY_PATH

CLOUD_URL_FILE="/root/presto/bin/cloud_url"
AUTH_TOKEN_FILE="/root/presto/bin/auth_token"

# misc URL parameters
CGI_URL="http://127.0.0.1:8001/getalp"

#################################################################
#  internal funtions/procedures
#################################################################


#issue ping sound

ping_command () {
PING_COMMAND="$PRESTO_PATH/bin/curl 'http://127.0.0.1:8001/playsndfile?file=$SOUND_FILE&async=0' 2>/dev/null"
echo "<-" $PING_COMMAND;
r=$(eval $PING_COMMAND);
}


SOUND_FILE=$1;
while [ "1" == "1" ];
do
    if [ -f $AUTH_TOKEN_FILE ];
    then
            echo "No Sound."
            exit 1;
    else
            ping_command;
            sleep 3;
    fi;
done;