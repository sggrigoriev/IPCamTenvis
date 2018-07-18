#!/bin/ash
cd /mnt/mtd;
rm -f cfg/*
tar xzvf cfg.tar.gz
cd /root/presto/bin
rm cloud_url;
rm auth_token;
printf "[config]\ncloud = \nkey = \nloc_id = 0\n" > /mnt/mtd/alp_tw.ini
reboot;
