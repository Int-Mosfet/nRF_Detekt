# nRF_Detekt

The best documention for this project will be the code, I like to comment and I feel like my code has been extra readable for this project.  This is meant as an add-on of sorts to existing security systems.  It's nice having the code and being able to flash MCU.  

Using nRF24L01+ radio modules nRF_Detekt send remote logs of activating a sensor from a transmitter connected to sensor; to an ideally hidden receiver.  If device uses wire as transmission line, it would be easy for a physical attacker to "follow the line" to logging device.

I won't move development to Atmel Studio 7 for this project.  But I would like more debugging capabilities (and just a better IDE) in the future.

Add in these libraries to the Arduino IDE to build: 

==> RF24: https://github.com/tmrh20/RF24/

==> Entropy: https://sites.google.com/site/astudyofentropy/project-definition/timer-jitter-entropy-sources/entropy-library

==> XTEA: https://github.com/franksmicro/Arduino/tree/master/libraries/Xtea

==> AESLib: https://github.com/DavyLandman/AESLib

==> LiquidCrystal_I2C: https://arduino-info.wikispaces.com/LCD-Blue-I2C?responseToken=1e511cd9cf16df3bfb9d039fb46f4752

Currently using AES-128-ECB, then XTEA, then AES-128-ECB encryption of a 32 byte struct: a microsecond timer, 4 entropy samples, a millisecond timer, and 2 authentication variables as the packet being sent back to the receiver, whenever a sensor changes state.  This should mean that the same encrypted data would be sent very rarely, if at all.  Also, the 2 authentication variables means not just anything can be sent to the receiver.  I kept going back and forth between more authentication but more similar messages, and less authentication but less similar messages (since I'm using AES-128-ECB) but settled on less authentication.  I'm not sure how to do IV's for AES-CBC between radios just yet (I can say what I want, just not sure how to do it).

It will use a AMS1117 5V-3.3V regulator module, to supply the radio with enough power.  As is, just using the 3.3V output pin on the Arduino does not supply enough current for the radio (supplies max of 50mA, we need about 130mA or so).  You could also use a transistor as an amplifier for the power, or use a completely separate power supply like a LM317T (probably the best, safest, most secure option but I'm choosing not to do that for "looks" and keeping everything compact).  

My WishList is the following: 

1) Want to support more better and stronger ciphers/modes.

2) Want to make a "shield" for holding the nRF24 module and a separate power supply.

3) Want some kind of "spread spectrum" implementation integrated in, to change channels on every send.

4) Want to incorporate some kind of authentication chip, like the ATSHA204A.

5) Want a battery backup power system that switches on if AC power is lost.

/* Development will happen sporatically, but mostly thru the summer and winter break times.  I want to move to some other projects too.  But I will keep revisiting this project because I know it can eventually be really good. */


![nrfd_p1](http://1.bp.blogspot.com/-XRwyKl3rFLQ/VrG48nReEUI/AAAAAAAAANI/AyTfjKfyB_s/s1600/0101161626.jpg)

![nrfd_p2](http://4.bp.blogspot.com/-xkpkRvPDMsE/VrG4_sUPCWI/AAAAAAAAANM/GqbcLJq_DQE/s1600/0101161633.jpg)

![nrfd_p3](http://1.bp.blogspot.com/-APzVJiaRSlg/VrG5CoMjAJI/AAAAAAAAANQ/rdJeIdwVzW8/s1600/0119160345.jpg)

![nrfd_p4](https://1.bp.blogspot.com/-V6RRoeGqZPg/V2uCpYnySYI/AAAAAAAAAN8/IaAWHKK6qqYHWNv0ph-Pu7p8cYir21IUQCLcB/s1600/nrf_detekt_pinout_recv.png)

![nrfd_p5](https://3.bp.blogspot.com/-Q-tvfZppnik/V2uCptDcHRI/AAAAAAAAAOE/gbL02jddSIcf1gVidwmsiHgbPR11BbApwCLcB/s1600/nrf_detekt_pinout_trans.png)

![nrfd_p6](https://1.bp.blogspot.com/-eC6pKo5zhXY/V3gafeqXVII/AAAAAAAAAOY/qT0lQnu-qJ406bZvJfdvNyXJu8U531yaQCLcB/s1600/0702161534%255B1%255D.jpg)
