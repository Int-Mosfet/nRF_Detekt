# nRF_Detekt
Using nRF24L01+ radio modules to send remote logs of activating a sensor from transmitter connected to sensor to an ideally hidden receiver.

I will likely soon move development to Atmel Studio 7, but...

Add in these libraries to the Arduino IDE to build: 

Uses RF24: https://github.com/tmrh20/RF24/

Uses Entropy: https://sites.google.com/site/astudyofentropy/project-definition/timer-jitter-entropy-sources/entropy-library

Uses XTEA: https://github.com/franksmicro/Arduino/tree/master/libraries/Xtea

Currently using XTEA encryption of a microsecond timer and a random(?) sample as the packet being sent back to the receiver, whenever a sensor changes state.  This should mean that the same encrypted data would be sent very rarely, if at all.

Want to support a few different ciphers, like Keeloq, and AES-128-CBC.

Want to make a "shield" for holding the nRF24 module and a separate power supply.

Want some kind of "spread spectrum" implementation integrated in, to change channels on every send.

Want a chat-based program, that works out the box.

