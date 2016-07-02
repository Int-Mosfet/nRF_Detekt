
/*
* Modified version of "Getting Started" example by TMRh20 from RF24 library
* https://github.com/TMRh20/RF24/blob/master/examples/GettingStarted/GettingStarted.ino
* 
* by Int-Mosfet, July 2016
* MIT License on: https://github.com/Int-Mosfet/nRF_Detekt/blob/master/LICENSE
*
* This code receives encrypted data, decrypts a struct consisting of 8 4-byte
* unsigned longs: 1 microsecond counter, 1 millisecond counter, 4 pseudo-random
* samples, and 2 pre-defined authentication variables; and logs how
* many times this happens using the internal EEPROM (displayed on an LCD)
*/

/**************
* nRF_Detekt (Receiver), V1.1
**************/

#include <SPI.h>                //for nRF module
#include <Wire.h>               //for LCD (uses i2c)
#include <LiquidCrystal_I2C.h>  //for LCD
#include <EEPROM.h>             //for storing channels, activations, other variables
#include <Entropy.h>            //for generating entropy
#include <AESLib.h>             //for AES encryption
#include <Xtea.h> 		//for XTEA encryption
				//Make sure to change WProgram.h to Arduino.h in xtea.cpp and xtea.h !!!!
#include <Keeloq.h>             //for Keeloq encryption
                                //Make sure to change WProgram.h to Arduino.h in Keeloq.cpp and Keeloq.h !!!!
#include "RF24.h"		//for nRF24 radio
#include "nRF24L01.h"		//for nRF24 radio
//#include "printf.h"

/**************
* CRYPTO VARIABLES
**************/
//XTEA block size is 8 bytes, note in version 1.0
//that I only encrypted 2 4-byte variables.
//"data" is a chunk of serialized data from
//a struct we want to encrypt, block-by-block.
//This way we can use the full packet size of
//the nRF24L01+ and add in some additional 
//authentication variables.
//"buff" is the final variable to put either
//encrypted/decrypted data into, fed into
//radio.write() function, transmitted, 
//received, then placed back into our dataPacket struct
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
//this is critical for high reliability,
//if the key changes and there's a power outage
//in the microseconds before RX receives the new key or IV
//then RX won't be able to decrypt.
//It uses a 64 bit key
//It uses a non-linear feedback shift register
//Frankly, it's a strange algorithm
#define KEELOQ_BLOCK_SIZE 4
byte data3[KEELOQ_BLOCK_SIZE];


/**************
* RADIO && LCD CONFIG I
**************/
// Used to control whether this node is sending or receiving, 1->TX, 0->RX
bool radioNumber = 0;  //Receiver
/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus (digital) pins 9 & 10 */
RF24 radio(9,10);
//byte addresses[][6] = {"1Node","2Node"};  //Max of 1,099,511,627,775 addresses (5 bytes)
const uint64_t addresses[2] = { 0xAABBCCDDEELL, 0xEEDDCCBBAALL };
//Set pins on i2c chip for LCD (most use address 0x27, mine uses 0x3F)
//i2c LCD only uses 4 pins...
//                    addr| en|rw|rs|d4|d5|d6|d7|bl|blpol
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); 

/**************
* GLOBAL VARIABLES
**************/
uint32_t act_count;			//activation count
uint16_t act_count_addr = 128;		//act count eeprom address
uint8_t ch_index;			//channel index
uint16_t ch_index_addr = 64;		//channel index eeprom address
uint8_t auth_fail_flag;			//if authentication fails, set this flag
uint16_t auth_fail_flag_addr = 256;	//auth fail flag eeprom address

uint16_t addr = 0;			//EEPROM address
uint8_t val = 0xC1;			//variable signifying activation in EEPROM
const uint8_t eeprom_state = 0;		//whether or not to write to eeprom on activations (0 -> off, 1 -> on)

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
unsigned long key[4] = { 0xDEADBEEF, 0xCAFEBEEF, 0xBABEBEEF, 0xF00DBEEF };  //mmm beefy...Change the keys
Xtea x(key);

//AES
uint8_t AES_key[] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };  //CHANGE. THIS. KEY. (16 bytes)
uint8_t AES_eeprom_key[] = {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31}; //AND THIS ONE. (16 bytes) (unused for now)

//Keeloq
//a high and low unsigned long as the key (64bits)
Keeloq k(0x12345678, 0x12345678);  //Do I have to say it?  Change the key.


/**************
 * SETUP()
 *************/
void setup() {

  //----------------------*IMPORTANT*---------------------------
  //If you need to clear out your EEPROM, just do it once
  //I don't want to add a little button push, clear eeprom feature
  //as this could cause lots of problems, especially security ones.
  //comment out after flashing the arduino 1st time before deploying
  //to clear out eeprom
  
  //for(int k = 0; k < EEPROM.length(); k++){
  //  EEPROM.write(k,0);
  //}
  //------------------------------------------------------------
  
  /*Getting variables from EEPROM*/
  ch_index = EEPROM.read(ch_index_addr);
  act_count = EEPROM.read(act_count_addr);
  auth_fail_flag = EEPROM.read(auth_fail_flag_addr);
  
  /************
  * RADIO && LCD CONFIG II
  ************/
  //init for 16 chars, 2 lines, turn on backlight
  lcd.begin(16,2);
  //delay(300);
  //lcd.noBacklight();				//turn off backlight
  lcd.setCursor(0,0);				//setting to char 0, line 0
  lcd.print("nRF_Detekt, V1.1");
  lcd.setCursor(0,1);				//setting to char 0, line 1
  lcd.print("Ch:");
  lcd.setCursor(3,1);
  lcd.print(ch_index);				//printing channel number
  lcd.setCursor(7,1);
  lcd.print("AN:");
  lcd.setCursor(10,1);
  lcd.print(act_count);				//printing activation number
  
  Serial.begin(115200);				//for the serial monitor
  
  radio.begin();			//turn on radio
  //radio.setChannel(ch_index);		//uncomment if you want channel changing feature
  radio.setChannel(0);			//otherwise statically set channel, I recommend using one from my list if in US
  radio.setAutoAck(1);			//This makes your life easier, but can potentially open up some attacks
  radio.setRetries(15, 15);		//largest retries possible, 15 (delay of 4000uS) and 15 (number of retries if error encountered)
  radio.setPayloadSize(32);		//we want to use all 32 bytes of payload
  radio.setAddressWidth(5);		//max length for max authentication
  radio.setDataRate(RF24_250KBPS);	//slowest speed for max reliability
  radio.setCRCLength(RF24_CRC_16);	//highest CRC for max reliability
  radio.setPALevel(RF24_PA_MAX);	//(Levels go from 0 - 3, 3 is MAX)
  
  // Open a writing and reading pipe on each radio, with opposite addresses
  // You can have a "multiceiver" where 1 receiver can be paired with up to
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
}//End setup()

/**************
 * LOOP()
 *************/
void loop() {

  act_count = EEPROM.read(act_count_addr);
  int timeout_count = 0;
  int channel;
  
  //intermediate variables for decryption
  char *headAddress = (char *)&dataPacket;
  char *buffAddress = (char *)&buff;
  
  //authentication check
  //no way to reset flag besides reprogramming
  //unless there's a backdoor
  //if adversary reprograms device, that's a physical attack
  //if you want to make it hard to reprogram, it would
  //be more secure but you may waste hardware on a benign
  //RF error, frustrating...
  auth_fail_flag = EEPROM.read(auth_fail_flag_addr);
  if(auth_fail_flag)
  {
	  while( 0 < 1 ) //hopefully that's an eternity :p
	  {
		  lcd.clear();
		  lcd.setCursor(0,0); //setting to char 0, line 0
		  lcd.print("---AUTH FAIL---");
		  lcd.setCursor(0,1);
		  lcd.print("--RESET DEVICE--");
	  }
  }

  if( radio.available() )
  {
        while( radio.available() )     //read in packet
        {
            radio.read(&dataPacket, sizeof(dataPacket)); //read in the packet to fill in the struct
        }

        //lcd.backlight(); //turn on backlight
        lcd.clear();
        lcd.setCursor(0,0); //setting to char 0, line 1
        lcd.print("---Activation---");
        lcd.setCursor(0,1);
        lcd.print("----Detected----");
        
        
        act_count++;
        //putting latest act_count into eeprom
        EEPROM.put(act_count_addr, act_count);
        //act_count = EEPROM.read(act_count_addr);
        timeout_count++;

        Serial.print("Before Decrypt: ");
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
		
		    //memcpy(void* dest, const void* src, sizeof bytes to copy);

        //Keeloq decryption
        for(uint32_t i = 0; i < 8; i++)  //8*4 = 32 bytes
        {  
          memcpy(data3, headAddress+(i*KEELOQ_BLOCK_SIZE), KEELOQ_BLOCK_SIZE);
          for(uint8_t j = 0; j < 10; j++)
          {
            k.decrypt((uint32_t)data3);  //I'm doing 10 rounds of Keeloq (it's slow!)
            //can't do AES here, blocksizes are off, get bad decryptions
          }
          memcpy(buffAddress+(i*KEELOQ_BLOCK_SIZE), data3, KEELOQ_BLOCK_SIZE);
        }//End Keeloq decryption
        //memcpy(&dataPacket, &buff, sizeof(dataPacket)); //<--Don't reassemble yet
		
        //XTEA decryption
        for(uint32_t i = 0; i < 4; i++)  //4*8 = 32 bytes
        {  
          memcpy(data, headAddress+(i*XT_BLOCK_SIZE), XT_BLOCK_SIZE);
          for(uint8_t j = 0; j < 64; j++)
          {
            x.decrypt((uint32_t*)data);  //Recommended 64 rounds, there's no noticeable delay
            //can't do AES here, blocksizes are off, get bad decryptions
          }
          memcpy(buffAddress+(i*XT_BLOCK_SIZE), data, XT_BLOCK_SIZE);
        }//End XTEA decryption
        //memcpy(&dataPacket, &buff, sizeof(dataPacket)); //<--Don't reassemble yet

        //AES-128-ECB decryption
        for(uint32_t i = 0; i < 2; i++)  //2*16 = 32 bytes
        {  
          memcpy(data2, headAddress+(i*AES_BLOCK_SIZE), AES_BLOCK_SIZE); 
          for(uint8_t j = 0; j < 10; j++)
          {
			      //recommended 10 rounds
            aes128_dec_single(AES_key, (uint32_t*)data2);
          }
          memcpy(buffAddress+(i*AES_BLOCK_SIZE), data2, AES_BLOCK_SIZE);
        }//End AES-128-ECB decryption
        
        memcpy(&dataPacket, &buff, sizeof(dataPacket)); //Now reassemble, placing variables back into struct
        

        Serial.print("After Decrypt: ");
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

        //authentication check
        if ( (dataPacket.d != 0xDEADBEEF) || (dataPacket.e != 0xBEEFDEAD) )
           {
              	//if you enter here it's bad news...
		//there may have been an attack, more likely
		//it's just a failure of the RF comms...
		//if there was an attack, it failed b/c of
		//the authentication check, now we can log this
		//potential attack.
			  
		act_count--; //don't log false activations
			  
              	//putting latest act_count into eeprom
              	EEPROM.put(act_count_addr, act_count);
              	//act_count = EEPROM.read(act_count_addr);
			  
              	//possibly have a separate variable to signify
              	//potential attacks in eeprom
		//for now let's just set another flag in eeprom
		//so that we have to reflash it to turn it off
			  
		auth_fail_flag = 1;
		EEPROM.put(auth_fail_flag_addr, auth_fail_flag);
           }
        
        
        Serial.print("Activation number: ");
        Serial.print(act_count);
        Serial.println();
        Serial.print("Current Channel Number: ");
        Serial.print(ch_index);
        Serial.println();
        Serial.println();
        
        

        //If you want to use nRF_Detekt how it was originally intended,
        //which was to write to the internal eeprom if an activation was
        //detected, then set eeprom_state to 1.
        //HOWEVER, you would probably want to simply choose an address
        //and use the EEPROM.put() function instead of the simplest method
        //I choose to start with, and just keep track of an activation number.
        //That takes up less space, but spread writes out (we have ~100,000 writes)
        //so not too much of a worry.
        
        if(eeprom_state)
        {
    
          EEPROM.write(addr, val); // write activation to EEPROM
          addr++; //increment address
    
          //if( addr == EEPROM.length()) //checking for overflow, Use this, I'm building on an older version of toolchain, so need to explicitly state length
          if( (addr >= 0x400) || (addr < 0) ) //1K actually means 1024, not 1000, rookie mistake, and I'm doing a slightly safer check, in case check gets bypassed 1 time
          {
              addr = 0;
              //Signify overflow (likely won't need unless you leave somewhere for awhile)
              val = 0xAA; //anything different from original variable
          }
    
          delay(100); // needed for write times out of an abundance in caution
        }
        
        
    
        if( timeout_count > 0 )
        {
            radio.powerDown();
            delay(5000);
            timeout_count = 0;
            uint8_t flush_tx();  //clean out buffer space
            uint8_t flush_rx();  //clean out buffer space
            radio.powerUp();
            radio.startListening();
        }
        
		    //re-initialize after powercycling radio
        //ch_index++;
        //EEPROM.put(ch_index_addr, ch_index);
        //ch_index = EEPROM.read(ch_index_addr);
        //reset radio parameters
        //radio.setChannel(ch_index);			//uncomment if you want channel changing feature
        act_count = EEPROM.read(act_count_addr);  	//keeping track of act_count
        radio.setChannel(0);				//otherwise statically set channel
        radio.setAutoAck(1);				//This makes your life easier, but can potentially open up some attacks
        radio.setRetries(15, 15);			//largest retries possible, 15 (delay of 4000uS) and 15 (number of retries if error encountered)
        radio.setPayloadSize(32);			//we want to use all 32 bytes of payload
        radio.setAddressWidth(5);			//max length for max authentication
        radio.setDataRate(RF24_250KBPS);		//slowest speed for max reliability
        radio.setCRCLength(RF24_CRC_16);		//highest CRC for max reliability
        radio.setPALevel(RF24_PA_MAX);			//(Levels go from 0 - 3, 3 is MAX)
        
	//uncomment if you want channel changing feature
        //if( ch_index > 124 ) //have to make it 1 less than TX index check to stay synced
        //{
        //    ch_index = 0;
        //    EEPROM.put(ch_index_addr, ch_index);
        //}

        if( act_count > 999999 ) //for the LCD screen, I only have 6 spaces
        {
          act_count = 0;
          EEPROM.put(act_count_addr, act_count);
        }

        lcd.clear();
        lcd.setCursor(0,0); //setting to char 0, line 0
        lcd.print("nRF_Detekt, V1.1");
        lcd.setCursor(0,1); //setting to char 0, line 1
        lcd.print("Ch:");
        lcd.setCursor(3,1);
        lcd.print(ch_index);  //printing channel number
        lcd.setCursor(7,1);
        lcd.print("AN:");
        lcd.setCursor(10,1);
        lcd.print(act_count); //printing activation number
        
    
  } //End if(radio.available())

} //End loop()


//Notes:
/*
Channels to use for Freq. hopping:

2400
2425   //safe channel (inbetween common wifi freqs)
2450   //safe channel (inbetween common wifi freqs)
2475   //not used in USA
2480   //not used in USA
2485   //not used in USA
2490   //not used in USA
2495   //not used in USA
2500   //safe channel (inbetween common wifi freqs)
2505
2510
2515
2520
2525
*/

