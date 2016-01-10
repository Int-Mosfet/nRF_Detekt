# nRF_Detekt
Using nRF24L01+ radio modules to send remote logs of activating a sensor

Currently using XTEA encryption of a microsecond timer and a random(?) sample as the packet being sent back to the receiver, whenever a sensor changes state.  This should mean that the same encrypted data would be sent very rarely, if at all.

Want to support a few different ciphers, like Keeloq, and AES-128-CBC.

Want to make a "shield" for holding the nRF24 module and a separate power supply.

Want some kind of "spread spectrum" implementation integrated in, to change channels on every send.
