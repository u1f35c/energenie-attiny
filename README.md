# energenie-attiny

This is a basic firmware for [Digispark](http://digistump.com/products/1) boards to emulate a 4 relay _www.dcttech.com_ device and control a set of [Energenie](https://energenie4u.co.uk/) [ENER002](https://energenie4u.co.uk/catalogue/product/ENER002-4) sockets via a cheap [FS1000A](https://www.ebay.co.uk/itm/292567243194) 433MHz transmitter. It uses [V-USB](https://www.obdev.at/products/vusb/) as its core.

`apt install avr-libc avrdude` should install the appropriate build requirements on Debian (`gcc-avr` will be automatically pulled in) assuming you already have `build-essential` installed for `make`.

`make` will then build you a main.hex which you can program to the Digispark using [micronucleus](https://github.com/micronucleus/micronucleus):

    micronucleus main.hex

Hooking up the FS1000A to the Digispark is easy; GND + 5V get connected up and then D0 is the data line. Bad ASCII art diagram:

<pre>
      DDDDDD              +-------+
      543210              |       |
           +----------+   |FS1000A|
     +-------+        |   |       |
     |oooooo |        |   |  ooo  |
     |       |        |   +-------+
3.3V |Digi  o|        +------+||
 GND |Spark o|----------------|+
  5V |      o|----------------+
     |       |               D5G
     +-+USB+-+               AVN
                             T D
                             A
</pre>

Darryl Bond's [usbrelay](https://github.com/darrylb123/usbrelay) will let you control things. The Energenie devices have a 20 bit unique code per remote, which is mapped to the serial number reported by the firmware. By default it is set to 12345; you can either sniff the signal from your existing setup and change this to match it, or the sockets can learn 2 different remotes so you can make something up and them make them learn it. ```usbrelay 12345_0=80ABC``` will set the serial number (and thus the 433MHz leader code) to 80ABC, for example.

The simple ```mqtt-power``` Python tool included in the repository shows how to easily control the relay from Python and hook it into an MQTT setup. You'll need ```python3-hid``` and ```python3-paho-mqtt``` installed under Debian.

Given that V-USB is GPLv2+ or commercial all of my code is released as GPLv3+, available at [https://the.earth.li/gitweb/?p=energenie-attiny.git;a=summary](https://the.earth.li/gitweb/?p=energenie-attiny.git;a=summary) or on GitHub for easy whatever at [https://github.com/u1f35c/energenie-attiny](https://github.com/u1f35c/energenie-attiny)
