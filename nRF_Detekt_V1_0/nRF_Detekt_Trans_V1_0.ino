/*
* Modified version of "Getting Started" example by TMRh20 from RF24 library (google it)
* by Int-Mosfet, Jan 2016
* MIT License on: https://github.com/Int-Mosfet/nRF_Detekt/blob/master/LICENSE
* 
* This code detects a state change from a sensor (in this case a radar with a relay that switches)
* and then encrypts a microsecond sample and a random unsigned long integer and sends out
* via an nRF24 radio module
*/

/**************
* nRF_Detekt (Transmitter), V1.0
**************/

#include <SPI.h>
#include <Entropy.h>
#include <Xtea.h>  //Make sure to change WProgram.h to Arduino.h in xtea.cpp and xtea.h !!!!
#include "RF24.h"
//#include "nRF24L01.h"
//#include "printf.h"

/**************
* RADIO CONFIG
**************/
/***      Set this radio as radio number 0 or 1         ***/
bool radioNumber = 1; //transmitter

/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 9 & 10 */
RF24 radio(9,10);

//Max of 1,099,511,627,775 addresses (5 bytes)
//byte addresses[][6] = {"1Node","2Node"};
const uint64_t addresses[2] = { 0xAABBCCDDEELL, 0xEEDDCCBBAALL };

// Used to control whether this node is sending or receiving, 1->TX, 0->RX
//bool role = 1;

//Counting radar activations
int act_count = 1;

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
unsigned long key[4] = { 0x4DEF9E08, 0x7DAEA7F3 , 0xC98497BC, 0xABCDABCD };
Xtea x(key);

void setup() {
  /* Use if you want to send dataPacket struct in the clear
  dataPacket.Act_num = 139; //value assigned to Act_num variable
  */
  
  Entropy.initialize();
  
  //randomSeed(analogRead(0)); //<--No! Don't use if you can avoid it
  
  
  //Radar init code
  pinMode(7, INPUT_PULLUP);
  //pinMode(13, OUTPUT);
  
  //initializing serial monitor use and turning on radio
  Serial.begin(115200);
  radio.begin();

 // Set the PA Level low to prevent power supply related issues since this is a
 // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  //radio.setPALevel(RF24_PA_LOW); //<-orig
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
}

void loop() {
  //Radar variables
  int sensorVal = digitalRead(7); //digital pin 7, relay from radar module connected here
  int timeout_count = 0;
  unsigned long random_long;
  random_long = Entropy.random() & 0xFFFFFF;
  unsigned long v[2] = { micros(), random_long };
  
  /* Use if you want to send dataPacket struct in the clear
  dataPacket.micro_sec_IV = micros();
  dataPacket.ran_byte_1 = random(255);
  delay(5);
  dataPacket.ran_byte_2 = random(255);
  delay(10);
  dataPacket.ran_byte_3 = random(255);
  */
  
  if(sensorVal == HIGH)
  {
    radio.startListening();
  }
  else
  {
    x.encrypt(v);
    Serial.println("After Encrypt: ");
    Serial.print(v[0], HEX);
    Serial.print(" ");
    Serial.print(v[1], HEX);
    Serial.println(" ");
    timeout_count++;
    radio.stopListening();
    Serial.println("Sending encrypted data...");
    
    //if (!radio.write( &dataPacket, sizeof(dataPacket) )) //<--Use for sending dataPacket struct
    if (!radio.write( &v, sizeof(v) ))
    {
      Serial.println("Send data failed");
    }
    
    if( timeout_count >0 )
    {
        radio.stopListening();
        delay(5000);
        timeout_count = 0;
        radio.startListening();
    }
    
  }

} //End Loop
