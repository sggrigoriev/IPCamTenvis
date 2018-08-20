#!/bin/ash
echo " <<<<<<<<<<<<<<<<<<<<<<<  RESETTING CAMERA to FACTORY STATE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" > /proc/self/fd/1
#cd /mnt/mtd;
#rm -f cfg/*
#tar xzvf cfg.tar.gz
#cd /root/presto/bin
#rm cloud_url;
#rm auth_token;
cp /root/presto/bin/alp_tw.ini /mnt/mtd
#printf "[config]\r\ncloud = \r\nkey = \r\n" > /mnt/mtd/alp_tw.ini
reboot;
