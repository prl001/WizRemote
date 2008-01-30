WizRemote v0.3 - remotely set timers on your Beyonwiz
Copyright 2008 Eric Fry
License: GPLv2 
efry@users.sourceforge.net

This is a proof of concept. It consists of two parts, a little daemon
process that runs on the Beyonwiz and a php web frontend. It's a bit rough
around the edges but I have been able to use it to set timers remotely on
the wiz!

This is alpha software please use at your own risk!

I'd be interested in any feedback you can give. I still need to find out
how to work out the channel data automatically.

--How it works--

The daemon accepts tcp/ip connections from the front end and either returns
the current timer list or adds a new timer to the list.

To add a new timer, the daemon rewrites the /tmp/config/book.xml file then
writes the /tmp/config directory to flash and reboots the machine.

--Installation--

To run this you will need telnet access to your wiz. 

Steps.

1. copy wizremote and reboot.sh to /tmp/mnt/idehdd/ on your wiz
2. run ./wizremote from the /tmp/mnt/idehdd dir 
3. setup and configure wizremote.php on your webserver. (You will need to
   set some timers on the wiz to gather the channel data needed for the
   config) I haven't worked out how the tsid,onid and svcid numbers are
   generated yet. :(

4. surf to wizremote.php then adjust the config with the channel numbers
   from your pre-existing timers.

--Warning--

Note! this is a very rough implemenation I'm not checking for overlapping
timers or dodgy input data. Please be careful when using it. I also have the
time shift buffer turned off as I don't clean this up when I reboot the
machine.


