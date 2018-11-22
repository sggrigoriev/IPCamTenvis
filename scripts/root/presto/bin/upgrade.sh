#!/bin/ash


#
#
#


source /root/presto/bin/presto.conf

export PATH=$PRESTO_PATH/bin:$PATH
export LD_LIBRARY_PATH=$PRESTO_PATH/lib:/mnt/mtd/lib/:$LD_LIBRARY_PATH

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
    echo "******** OTA upgrade file exists, preforming upgrade ******" > /dev/consloe
    cd $UPGRADE_DIR
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
    echo " ****** No OTA Upgrade file $UPGRADE_DIR/$OTA_UPGRADE_FILE exists, proceeding *******" > /dev/console
fi
}


firmware_upgrade;