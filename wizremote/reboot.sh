#!/bin/sh
/usr/bin/killall sleep
/usr/bin/killall wizpimp
/usr/bin/killall wizmon
/bin/kill -HUP `cat /tmp/wizdvp.pid`

/bin/touch /tmp/mnt/idehdd/.grace
/bin/sync                        

#save config

/flash/bin/wizdvp_cramfs /tmp/config /tmp/config.tgz
if [ "$?" = "0" ]; then
	/usr/local/bin/flash_write /dev/mtd/2 /tmp/config.tgz;
elif [ "$?" = "1" ]; then
	/usr/local/bin/flash_write /dev/mtd/3 /tmp/config.tgz;
else
	echo "making cramfs image failed";
fi

/bin/sync                
/bin/sleep 2

/bin/micomparam -r 530101

