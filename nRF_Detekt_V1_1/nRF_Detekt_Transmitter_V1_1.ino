
/*
* Modified version of "Getting Started" example by TMRh20 from RF24 library
* by Int-Mosfet, July 2016
* 
* https://github.com/TMRh20/RF24/blob/master/examples/GettingStarted/GettingStarted.ino
* 
* MIT License on: https://github.com/Int-Mosfet/nRF_Detekt/blob/master/LICENSE
* 
* This code detects a state change from a sensor (in my case a radar with a relay)
* and then encrypts a struct consisting of 8 4-byte unsigned longs: 
* 1 microsecond counter, 1 millisecond counter, 4 pseudo-random samples, and 2 pre-defined 
* authentication variables; and sends out via an nRF24 radio module.
* Optionally, the channel sent on is then incremented on each activation.
* The encryption is Keeloq -> XTEA -> AES-128-ECB
*/

/**************
* nRF_Detekt (Transmitter), V1.1
**************/

#include <SPI.h>		//for nRF module
#include <Entropy.h>		//for generating entropy
#include <EEPROM.h>		//for storing channels, activations, other variables
#include <AESLib.h>		//for AES encryption
#include <Xtea.h>		//for XTEA encryption
				//Make sure to change WProgram.h to Arduino.h in xtea.cpp and xtea.h !!!!
#include <Keeloq.h>       	//for Keeloq encryption
                          	//Make sure to change WProgram.h to Arduino.h in Keeloq.cpp and Keeloq.h !!!!
#include "RF24.h"		//for nRF24 radio (primary firmware in .cpp file)
#include "nRF24L01.h"		//for nRF24 radio (definitions)

/**************
* CRYPTO VARIABLES
**************/
//XTEA blocksize is 8 bytes, note in version 1.0
//that I could only encrypt 2 4-byte variables.
//"data" is a chunk of serialized data from
//a struct we want to encrypt, block-by-block.
//This way can use the full packet size of
//the nRF24L01+ and add in some additional
//authentication variables.
//"buff" is the final variable to put either
//encrypted/decrypted data into, fed into
//radio.write() function
#define XT_BLOCK_SIZE 8
byte data[XT_BLOCK_SIZE];
byte buff[32];

//AES blocksize is 16 bytes.
//An entirely separate encryption
//routine is needed or else I believe
//zeros are being added in to encryption
//then not being added before decryption,
//resulting in an incorrect decryption of the data.
//Doing AES-128-ECB for now since a
//random IV is needed for CBC mode
//which would have to be sent separately
//(which I'm not 100% sure how to do...securely)
#define AES_BLOCK_SIZE 16
byte data2[AES_BLOCK_SIZE];

//Keeloq blocksize is 4 bytes
//It's a symmetric block cipher just
//like how I'm using AES-ECB and XTEA
//symmetric means it uses the same key
//to encrypt and decrypt.
//This is critical for high reliability,
//if the key changes and there's a power outage
//in the microseconds before RX receives the new key or IV
//then RX won't be able to decrypt.
//It uses a 64 bit key
//It uses a non-linear feedback shift register
//Frankly, it's a strange algorithm
#define KEELOQ_BLOCK_SIZE 4
byte data3[KEELOQ_BLOCK_SIZE];


/**************
* RADIO CONFIG I
**************/
//Used to control whether this node is sending or receiving, 1->TX, 0->RX
bool radioNumber = 1; //transmitter

//Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

//Max of 1,099,511,627,775 addresses (5 bytes)
//byte addresses[][6] = {"1Node","2Node"};
const uint64_t addresses[2] = { 0xAABBCCDDEELL, 0xEEDDCCBBAALL };

/**************
* ACTIVATION COUNTER && CHANNEL INDEX
**************/
//Counting activations
uint32_t act_count;
//EEPROM address of act_count
uint16_t act_count_addr = 128;
//channel index
uint8_t ch_index;
//EEPROM address of ch_index
uint16_t ch_index_addr = 64;
//RF fail flag (to note either interference or potential attack)
//uint8_t RF_fail_flag;


/**************
* PACKET DATASTRUCTURES
**************/
// A 32 byte struct which is max packet size of nRF24L01+
// Thus 8 4-byte unsigned longs is max we can do
// for how I'm using the module right now
struct packetStruct{
  unsigned long a;      //uS timer 1
  unsigned long b;      //entropy sample 1
  unsigned long c;      //entropy sample 2
  unsigned long d;      //authenticate var 1
  unsigned long e;      //authenticate var 2
  unsigned long f;      //entropy sample 3
  unsigned long g;      //entropy sample 4
  unsigned long h;      //mS timer 2
} dataPacket;

//maybe try another struct if you want to send more
/*struct packetStruct{
  unsigned long i;      //
  unsigned long j;      //
  unsigned long k;      //
  unsigned long l;      //
  unsigned long m;      //
  unsigned long n;      //
  unsigned long o;      //
  unsigned long p;      //
} dataPacket2;*/


/**************
* ENCRYPTION KEYS
**************/
//XTEA
unsigned long key[4] = { 0xDEADBEEF, 0xCAFEBEEF, 0xBABEBEEF, 0xF00DBEEF }; //mmm beefy...Change the keys
Xtea x(key);

//AES
uint8_t AES_key[] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };  //CHANGE. THIS. KEY. (16 bytes)

//unused for now
uint8_t AES_eeprom_key[] = { 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31 };

//Keeloq
//a high and low unsigned long as the key (64bits)
Keeloq k(0x12345678, 0x12345678);  //Do I have to say it? Change the key.

/**************
* SETUP()
**************/
void setup() {
  
  //----------------------*IMPORTANT*---------------------------
  //If you need to clear out your EEPROM, just do it once
  //I don't want to add a little button push, clear eeprom feature
  //as this could cause lots of problems, especially security ones.
  //comment out after flashing the arduino 1st time before deploying
  //to clear out the eeprom
  //for(int k = 0; k < EEPROM.length(); k++){
  //  EEPROM.write(k,0);
  //}
  //------------------------------------------------------------
  
  /*Getting variables from EEPROM*/
  ch_index = EEPROM.read(ch_index_addr);
  act_count = EEPROM.read(act_count_addr);
  
  /**************
  * INITIALIZE ENTROPY SOURCE
  **************/
  Entropy.initialize();
  
  
  //Normally Open (NO) pin from sensor relay
  //I'm using digital pin 7
  //Common (COM) on the relay goes to Ground (GND)
  //Only takes 2 pins to hook up the relay to the Arduino
  pinMode(7, INPUT_PULLUP);
  
  //initializing serial monitor use and turning on radio
  Serial.begin(115200);
  /**************
  * RADIO CONFIG II
  **************/
  radio.begin();			//turn on radio
  //radio.setChannel(ch_index);		//uncomment if you want channel changing feature
  radio.setChannel(0);			//otherwise statically set channel, I recommend using one from my list if in US
  radio.setAutoAck(1);			//This makes your life easier, but can potentially open up some attacks
  radio.setRetries(15, 15);		//largest retries possible, 15 (delay of 4000us) and 15 (number of retries if error encountered)
  radio.setPayloadSize(32);		//we want to use all 32 bytes of payload
  radio.setAddressWidth(5);		//max length for max authentication
  radio.setDataRate(RF24_250KBPS);	//slowest speed for max reliability
  radio.setCRCLength(RF24_CRC_16);	//highest CRC for max reliability
  radio.setPALevel(RF24_PA_MAX);	//one level above MIN (Levels go from 0 - 3, 3 is MAX)
  
  
  // Open a writing and reading pipe on each radio, with opposite addresses
  // You can have a "multiceiver" where 1 receiver can be paired up to
  // 6 separate "transmitters" (in quotes because they're all transceivers)
  if(radioNumber){
    radio.openWritingPipe(addresses[1]);
    radio.openReadingPipe(1,addresses[0]);
  }else{
    radio.openWritingPipe(addresses[0]);
    radio.openReadingPipe(1,addresses[1]);
  }
  
  // Start the radio listening for data
  radio.startListening();
  
  
} //End setup()

/**************
* LOOP()
**************/
void loop() {
  
  uint8_t sensorVal = digitalRead(7); //digital pin 7, relay from radar module connected here
                                      //if your sensor sends out a digital signal then you can
                                      //use that too, that makes it even easier in fact
 
  //Test variables to verify crypto works
  /*
  dataPacket.a = 0xDEADDEAD;
  dataPacket.b = 0xBEEFBEEF;
  dataPacket.c = 0xBABEBABE;
  dataPacket.d = 0xCAFECAFE;
  dataPacket.e = 0xF00DF00D;
  dataPacket.f = 0xFACEFACE;
  dataPacket.g = 0xFEEBFEEB;
  dataPacket.h = 0xD00FD00F;
  */
  
  
  if(sensorVal == HIGH)
  {
    radio.startListening();
  }
  else
  {
    //I know it's weird, but putting a lot of variables
    //here makes the sensor not miss activations sometimes
    //because it takes too much time to do all those
    //initializations, and sometimes an activation gets
    //missed which is a big NO NO for this project
    //If I put them in setup() I get an "out of scope" compiler error
    
    //If you don't change these authentication
    //variables, the only other thing preventing
    //fraudulent activations is the channel and addresses
    //of radio modules
    unsigned long AUTHENTICATE1 = 0xDEADBEEF;
    unsigned long AUTHENTICATE2 = 0xBEEFDEAD;
  
    //intermediate variables for encryption
    char *headAddress = (char *)&dataPacket;
    char *buffAddress = (char *)&buff;
	
    //after every activation there will be a timeout period
    uint8_t timeout_count = 0;
	
    //dataPacket variables
    unsigned long timer1 = micros();
    unsigned long timer2 = millis();
    unsigned long random_long1;
    unsigned long random_long2;
    unsigned long random_long3;
    unsigned long random_long4;
    random_long1 = Entropy.random() & 0xFFFFFF;
    random_long2 = Entropy.random() & 0xFFFFFF;
    random_long3 = Entropy.random() & 0xFFFFFF;
    random_long4 = Entropy.random() & 0xFFFFFF;
  
    dataPacket.a = timer1;           //uS timer 1
    dataPacket.b = random_long1;     //entropy sample 1
    dataPacket.c = random_long2;     //entropy sample 2
    dataPacket.d = AUTHENTICATE1;    //authenticate var 1
    dataPacket.e = AUTHENTICATE2;    //authenticate var 2
    dataPacket.f = random_long3;     //entropy sample 3
    dataPacket.g = random_long4;     //entropy sample 4
    dataPacket.h = timer2;           //mS timer 2
    
    
    Serial.print("Before Encrypt: ");
    Serial.print(dataPacket.a, HEX);
    Serial.println();
    Serial.print(dataPacket.b, HEX);
    Serial.println();
    Serial.print(dataPacket.c, HEX);
    Serial.println();
    Serial.print(dataPacket.d, HEX);
    Serial.println();
    Serial.print(dataPacket.e, HEX);
    Serial.println();
    Serial.print(dataPacket.f, HEX);
    Serial.println();
    Serial.print(dataPacket.g, HEX);
    Serial.println();
    Serial.print(dataPacket.h, HEX);
    Serial.println();
    
    //What I'm doing is using a form of chaining ciphers, so if someone can break
    //XTEA, can they break Keeloq and AES too?  What about combining them together?
    //I'm very certain that hacks to nRF_Detekt will be because a weakness/flaw
    //in the enhanced shockburst protocol used by the radio modules, or an attack
    //on your sensor used, or killing power (I haven't set up a way to deal with
    //that yet, like a weird glitch) over breaking the encryption
    //memcpy( void* dest, const void* src, sizeof bytes to copy)
    
    //Keeloq encryption
    for( uint32_t i = 0; i < 8; i++)  //8*4 = 32 bytes
    {
      memcpy(data3, headAddress+(i*KEELOQ_BLOCK_SIZE), KEELOQ_BLOCK_SIZE);
      for(uint8_t j = 0; j < 10; j++)
      {
        k.encrypt((uint32_t)data3);  //I'm doing 10 rounds of Keeloq (it's slow!)
      }
      memcpy(buffAddress+(i*KEELOQ_BLOCK_SIZE), data3, KEELOQ_BLOCK_SIZE);
    }
    //memcpy(&dataPacket, &buff, sizeof(dataPacket)); //<--Don't reassemble yet
    
    //XTEA Encryption
    for( uint32_t i = 0; i < 4; i++)  //4*8 = 32 bytes
    {
      memcpy(data, headAddress+(i*XT_BLOCK_SIZE), XT_BLOCK_SIZE);
      for(uint8_t j = 0; j < 64; j++)
      {
        x.encrypt((uint32_t*)data);  //Recommended 64 rounds
      }
      memcpy(buffAddress+(i*XT_BLOCK_SIZE), data, XT_BLOCK_SIZE);
    }
    //memcpy(&dataPacket, &buff, sizeof(dataPacket)); //<--Don't reassemble yet
    
    //AES Encryption
    for(uint32_t i = 0; i < 2; i++)  //2*16 = 32bytes
    {
      memcpy(data2, headAddress+(i*AES_BLOCK_SIZE), AES_BLOCK_SIZE);
      for(uint8_t j = 0; j < 10; j++)
      {
        aes128_enc_single(AES_key, (uint32_t*)data2); //Recommended 10 rounds
      }
      memcpy(buffAddress+(i*AES_BLOCK_SIZE), data2, AES_BLOCK_SIZE);
    }
    memcpy(&dataPacket, &buff, sizeof(dataPacket));  //Now reassemble for transmission
    
    Serial.print("After Encrypt: ");
    Serial.print(dataPacket.a, HEX);
    Serial.println();
    Serial.print(dataPacket.b, HEX);
    Serial.println();
    Serial.print(dataPacket.c, HEX);
    Serial.println();
    Serial.print(dataPacket.d, HEX);
    Serial.println();
    Serial.print(dataPacket.e, HEX);
    Serial.println();
    Serial.print(dataPacket.f, HEX);
    Serial.println();
    Serial.print(dataPacket.g, HEX);
    Serial.println();
    Serial.print(dataPacket.h, HEX);
    Serial.println();
    
    timeout_count++;
    act_count++;
    //putting latest act_count into eeprom
    EEPROM.put(act_count_addr, act_count);
    act_count = EEPROM.read(act_count_addr);
    //have to stop listening to transmit
    radio.stopListening();
    Serial.println("Sending encrypted data...");
    Serial.print("Activation counter: ");
    Serial.print(act_count);
    Serial.println();
    Serial.print("Current Channel Number: ");
    Serial.print(ch_index);
    Serial.println();
    Serial.println();
 
    
    if(!radio.write(&dataPacket, sizeof(dataPacket)) )
    {
      Serial.println("Send data failed");
      //if( index > 125 )        //keep scanning channels
      //  {
      //    index=0;
      //  }
      
      
      
      //index--; //Subtract one, to keep channels aligned
      
      //write it to eeprom
      //EEPROM.put(ch_index_addr, ch_index);
      //Will probably keep track of failures via a flag to retransmit or writing to eeprom
      //some method/algorithm to keep repeating the activation if we reach this fail state
      //probably for V1.2, if I can think up something sufficient
    }
    
    if( timeout_count > 0 )
    {
        radio.powerDown();
        delay(5000);			//this delay is because one radar I'm
					//using pulls line low *anytime*, so it
					//results in way more
					//activations than would happen
        timeout_count = 0;
        uint8_t flush_tx();		//clean out buffer space
        uint8_t flush_rx();		//clean out buffer space
        radio.powerUp();
        radio.startListening();
    }
    //re-initialize after powercycling radio
    //ch_index++;
    //EEPROM.put(ch_index_addr, ch_index);
    //ch_index = EEPROM.read(ch_index_addr);
    //radio.setChannel(ch_index);		/uncomment if you want channel changing feature
    radio.setChannel(0);			//otherwise statically set channel
    radio.setAutoAck(1);			//This makes your life easier, but can potentially open up some attacks
    radio.setRetries(15, 15);			//largest retries possible, 15 (delay of 4000us) and 15 (number of retries if error encountered)
    radio.setPayloadSize(32);			//we want to use all 32 bytes of payload
    radio.setAddressWidth(5);			//max length for max authentication
    radio.setDataRate(RF24_250KBPS);		//slowest speed for max reliability
    radio.setCRCLength(RF24_CRC_16);		//highest CRC for max reliability
    radio.setPALevel(RF24_PA_MAX);		//one level above MIN (Levels go from 0 - 3, 3 is MAX)
    
    //uncomment for channel changing feature
    //if( ch_index > 125 )
    //{
    //    ch_index=0;
    //    EEPROM.put(ch_index_addr, ch_index);
    //}
    
  } //End else

} //End loop()

