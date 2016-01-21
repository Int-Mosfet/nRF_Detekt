/*
* Modified version of "Getting Started" example by TMRh20 from RF24 library (google it)
* by Int-Mosfet, Jan 2016
* MIT License on: https://github.com/Int-Mosfet/nRF_Detekt/blob/master/LICENSE
*
* This code receives encrypted data, decrypts a microsecond sample and
* a random unsigned long integer, then writes a value to internal EEPROM
*/

/**************
* nRF_Detekt (Receiver), V1.0
**************/

#include <SPI.h>
#include <EEPROM.h>
#include <Entropy.h>
#include <Xtea.h>  //Make sure to change WProgram.h to Arduino.h in xtea.cpp and xtea.h !!!!
#include "RF24.h"
#include "nRF24L01.h"
#include "printf.h"

/**************
* RADIO CONFIG
**************/
/***      Set this radio as radio number 0 or 1         ***/
bool radioNumber = 0;  //Receiver
/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus (digital) pins 9 & 10 */
RF24 radio(9,10);

/**************
* GLOBAL VARIABLES
**************/
//byte addresses[][6] = {"1Node","2Node"};  //Max of 1,099,511,627,775 addresses (5 bytes)
const uint64_t addresses[2] = { 0xAABBCCDDEELL, 0xEEDDCCBBAALL };
bool role = 0;  // Used to control whether this node is sending or receiving, 1->TX, 0->RX
int act_count = 0;  //activation count
int addr = 0;  //EEPROM address
int val = 0xC1;  //variable signifying activation in EEPROM

/**************
* PACKET DATASTRUCTURES
**************/
/* Use if you want to send dataPacket struct in the clear
struct packetStruct{
  unsigned long micro_sec_IV;
  uint8_t Act_num;
  uint8_t ran_byte_1;
  uint8_t ran_byte_2;
  uint8_t ran_byte_3;
} dataPacket; 
*/

/**************
* ENCRYPTION KEYS
**************/
unsigned long key[4] = { 0x4DEF9E08, 0x7DAEA7F3, 0xC98497BC, 0xABCDABCD };
Xtea x(key);

void setup() {
  //dataPacket.Act_num = 139; //Value to check for activation
  
  /************
  * SETTING UP RADIO
  ************/
  //Will configure radio more completely/customly in later versions, using lots of defaults for now
  //radio.setAutoAck(1);
  //radio.disableAckPayload(); //not a function name, but find how to turn off ack for stealth mode only
  //radio.setRetries(0, 3); //smallest time between retries, 3 retries (max 15)
  //radio.setPayloadSize(32);
  //radio.setAddressWidth(5); //max length
  
  Serial.begin(115200);
  
  radio.begin();

  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  //radio.setPALevel(RF24_PA_LOW);
  radio.setPALevel(1); //one level above MIN (Levels go from 0 - 3, 3 is MAX)
  
  // Open a writing and reading pipe on each radio, with opposite addresses
  if(radioNumber){
    radio.openWritingPipe(addresses[1]);
    radio.openReadingPipe(1,addresses[0]);
  }else{
    radio.openWritingPipe(addresses[0]);
    radio.openReadingPipe(1,addresses[1]);
  }
  
  // Start the radio listening for data
  radio.startListening();
}//End setup()

void loop() {
int timeout_count = 0;
unsigned long v[2];

if( radio.available())
{
        while( radio.available())
        {
            //radio.read(&dataPacket, sizeof(dataPacket)); //<---Use if you want to send dataPacket struct in the clear
            radio.read(&v, sizeof(v));
        }
        
        act_count++;
        timeout_count++;
        x.decrypt(v);
        Serial.println("After Decrypt: ");
        Serial.print(v[0], HEX);
        Serial.print(" ");
        Serial.print(v[1], HEX);
        
        /* Use for seeing received dataPacket struct
        Serial.println("IV Number: ");
        Serial.println(dataPacket.micro_sec_IV);
        Serial.println("Act_num: ");
        Serial.println(dataPacket.Act_num);
        Serial.println("Activation count: ");
        Serial.println(act_count);
        Serial.println("Random byte 1: ");
        Serial.println(dataPacket.ran_byte_1);
        Serial.println("Random byte 2: ");
        Serial.println(dataPacket.ran_byte_2);
        Serial.println("Random byte 3: ");
        Serial.println(dataPacket.ran_byte_3);
        */
        
        
        Serial.println(" ");
    
        EEPROM.write(addr, val); // write activation to EEPROM
        addr++; //increment address
    
        //if( addr == EEPROM.length()) //checking for overflow, Use this, I'm building on an older version of toolchain, so need to explicitly state length
        if( addr == 0x3E8 ) //hex for 1000, 1k EEPROM
        {
            addr = 0;
            //Signify overflow (likely won't need unless you leave somewhere for awhile)
            val = 0xAA; //anything different from original variable
        }
    
        delay(100); // WATCHME: Was in example.  May be needed for unknown technical reason (write times...), may not be needed, test for it
    
        if(timeout_count > 0)
        {
            radio.stopListening();
            delay(5000);
            timeout_count = 0;
            radio.startListening();
        }
    
} //End if(radio.available())

} //End loop()
