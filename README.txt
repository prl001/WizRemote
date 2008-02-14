WizRemote v0.6a - remotely set timers on your Beyonwiz
Copyright 2008 Eric Fry
License: GPLv2 
efry@users.sourceforge.net

This is a proof of concept. It consists of two parts, a little daemon
process that runs on the Beyonwiz and a php web frontend. It's a bit rough
around the edges but I have been able to use it to set timers remotely on
the wiz!

This is alpha software please use at your own risk!

I'd be interested in any feedback you can give.

--How it works--

The daemon accepts tcp/ip connections from the front end and either returns
the current timer list or adds a new timer to the list.

To add a new timer, the daemon rewrites the /tmp/config/book.xml file then
writes the /tmp/config directory to flash and reboots the machine.

--Installation--

To run this you will need telnet access to your wiz. 

Steps.

1. copy wizremote, wizremote.key and reboot.sh to /tmp/mnt/idehdd/wizremote/ on your wiz
2. run ./wizremote from the /tmp/mnt/idehdd/wizremote dir 
3. setup and configure wizremote.php on your webserver. You will need to export your
   channel list from the wiz. This can be done by running the update_channels.php script
   on the website.

--Warning--

Note! this is a very rough implemenation I'm not checking for overlapping
timers or dodgy input data. Please be careful when using it. I also have the
time shift buffer turned off as I don't clean this up when I reboot the
machine.

The files reboot.sh and rc.local must be in unix text format!

	
