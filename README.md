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

jack-volume can receive OSC over UDP and TCP at the same time if the network ports are available on your system. The default port for both UDP and TCP is 7600 but you can configure that to your needs.
The OSC address of jack-volume consists of a static domain part and a static application part and the dynamic JACK client name and the dynamic channel number including "master".
The static part of the OSC address is fixed and can not be configured. But you can adjust the client name which has to be unique on one single machine.

For example:

|    domain    | application |  client-name | channel |
|:------------:|:-----------:|:------------:|:-------:|
| /org/example |   /volume   | /jack-volume | /master |
| /org/example |   /volume   | /jack-volume | /0      |

At the moment jack-volume expects a single float parameter between 0.0 and 1.0 representing the absolute gain to apply. Your OSC client is responsible to convert decibel values to the jack-volume gain range.

Later on I will probably implement a mute functionallity to mute and unmute channels independent from the channel gain.

How To Install
--------------

This software requires the [libOSC++](https://github.com/voidseg/libOSCpp) library I published in another [github repository](https://github.com/voidseg/libOSCpp). Install this library first with `configure`, `make`, `make install`. When building jack-volume, it will be linked to the static lib binary of libOSC++. Also make sure to install the JACK development packages.

Download the jack-volume archive or clone the git repository. Build and install with:

```
$ make
# make install
```

Maybe the install process will be improved sometime in the future.

How to Use
----------
First start a JACK server. Then you should be able to run jack-volume by simply typing `jack-volume` in a terminal. jack-volume is configured only by its parameters.

Usage:

`$ jack-volume [-c <jack_client_name>] [-s <jack_server_name>] [-p <osc_port>] [-n <number_of_channels>]`

Quit jack-volume by typing CTRL-C.
