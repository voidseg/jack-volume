jack-volume
===========

JACK client for controlling the volume of multiple audio channels via OSC.

The Idea
--------

jack-volume is a tool to control the volume of [JACK](http://jackaudio.org/ "JACK Website") clients. Both JACK and jack-volume follow the UNIX philosophy of one tool per job. JACK has nothing to do with audio signal processing but only does routing between audio interfaces and software clients. So I created jack-volume which can control the volume of JACK connections.

How It Works
------------

jack-volume connects to a JACK server and offers a variable number of audio channels. It does no audio mixing at all but just forwards the input channels to the corresponding output channels. jack-volume applies a gain to each channel and additionally a master gain to all channels. The gains are controlled via an OSC interface.

The OSC Part
------------

[OSC](http://en.wikipedia.org/wiki/Open_Sound_Control "Wikipedia - OSC") is a nice protocol that should replace MIDI and becomes more popular every day. All OSC messages contain an address string and some parameters of OSC specific data types. For more information to OSC please read the official documentation.

If possible, jack-volume listens on UDP and TCP at once. The default port for both protocols is 7600 but that can be configured to your needs.
The OSC address jack-volume is using cannot be configured and is unique for every jack-volume process on a single machine. It consists of a static domain part and a static application part and the dynamic JACK client name and the dynamic channel number or "master" for all channels.

For example:

|    domain    | application |  client-name | channel |
|:------------:|:-----------:|:------------:|:-------:|
| /net/mhcloud |   /volume   | /jack-volume | /master |
| /net/mhcloud |   /volume   | /jack-volume | /0      |

At the moment jack-volume expects a single float parameter between 0.0 and 1.0 representing the absolute gain to apply. Your OSC client is responsible to convert decibel values to the jack-volume gain range.

Later on I will probably implement a mute functionallity to mute and unmute channels independent from the channel gain.

How To Install
--------------

This software requires the libOSC++ library I published in another github repo. Install this library first with configure, make, make install. jack-volume will be linked to the static library of libOSC++. Also make sure to install the JACK development packages.

Download the jack-volume archive or clone the repository wth git. Build and install with:

`make`

`make install`

Maybe the install process will be improved sometimes in the future.

How to Use
----------
You should be able to run jack-volume by simply type `jack-volume` in a terminal. jack-volume is configured only by its parameters.

Usage:

`jack-volume [-c <jack_client_name>] [-s <jack_server_name>] [-p <osc_port>] [-n <number_of_channels>]`

Quit jack-volume by typing CTRL-C.
