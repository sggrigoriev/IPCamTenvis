#!/bin/ash
cd /mnt/mtd;
rm -f cfg/*
tar xzvf cfg.tar.gz
cd /root/presto/bin
rm cloud_url;
rm auth_token;
reboot;
