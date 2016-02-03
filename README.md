# nRF_Detekt

This is meant as an add-on of sorts to existing security systems.  It's nice having the code and being able to flash MCU.  

Using nRF24L01+ radio modules nRF_Detekt send remote logs of activating a sensor from a transmitter connected to sensor; to an ideally hidden receiver.  If device uses wire as transmission line, it would be easy for a physical attacker to "follow the line" to logging device.

I will likely soon move development to Atmel Studio 7, but...

Add in these libraries to the Arduino IDE to build: 

==> RF24: https://github.com/tmrh20/RF24/

==> Entropy: https://sites.google.com/site/astudyofentropy/project-definition/timer-jitter-entropy-sources/entropy-library

==> XTEA: https://github.com/franksmicro/Arduino/tree/master/libraries/Xtea

Currently using XTEA encryption of a microsecond timer and a random(?) sample as the packet being sent back to the receiver, whenever a sensor changes state.  This should mean that the same encrypted data would be sent very rarely, if at all.

Want to support a few different ciphers, like Keeloq, and AES-128-CBC.

Want to make a "shield" for holding the nRF24 module and a separate power supply.

Want some kind of "spread spectrum" implementation integrated in, to change channels on every send.

Want a chat-based program, that works out the box.

/* Development on hold until Summer 2016 as school and work are taking close to all of developer's time */


![nrfd_p1](http://1.bp.blogspot.com/-XRwyKl3rFLQ/VrG48nReEUI/AAAAAAAAANI/AyTfjKfyB_s/s320/0101161626.jpg)
:height: 500px
:width: 400px

