#!/bin/ash

source ./presto.conf

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
    fi;
done;