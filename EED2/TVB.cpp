/*
TV-B-Gone for Arduino version 1.2, Oct 23 2010
Ported to Arduino by Ken Shirriff=
http://www.arcfn.com/2010/11/improved-arduino-tv-b-gone.html

The original code is:
TV-B-Gone Firmware version 1.2
 for use with ATtiny85v and v1.2 hardware
 (c) Mitch Altman + Limor Fried 2009
 Last edits, August 16 2009


 I added universality for EU or NA,
 and Sleep mode to Ken's Arduino port
      -- Mitch Altman  18-Oct-2010
 Thanks to ka1kjz for the code for adding Sleep
      <http://www.ka1kjz.com/561/adding-sleep-to-tv-b-gone-code/>

 With some code from:
 Kevin Timmerman & Damien Good 7-Dec-07

  I made the code independent, to run it as a subroutine on another sketch
  Used #defines to choose EU or NA codes, to save program memory
  -- Mastro Gippo 2014-12-28

Distributed under Creative Commons 2.5 -- Attib & Share Alike

 */

#include "TVB.h"
#include <inttypes.h>
#include "Arduino.h"
#include <avr/pgmspace.h>
#include <p3310.h>


void xmitCodeElement(uint16_t ontime, uint16_t offtime, uint8_t PWM_code );
void delay_ten_us(uint16_t us);
uint8_t read_bits(uint8_t count);

#define putstring_nl(s) Serial.println(s)
#define putstring(s) Serial.print(s)
#define putnum_ud(n) Serial.print(n, DEC)
#define putnum_uh(n) Serial.print(n, HEX)

/*
This project transmits a bunch of TV POWER codes, one right after the other,
 with a pause in between each.  (To have a visible indication that it is
 transmitting, it also pulses a visible LED once each time a POWER code is
 transmitted.)  That is all TV-B-Gone does.  The tricky part of TV-B-Gone
 was collecting all of the POWER codes, and getting rid of the duplicates and
 near-duplicates (because if there is a duplicate, then one POWER code will
 turn a TV off, and the duplicate will turn it on again (which we certainly
 do not want).  I have compiled the most popular codes with the
 duplicates eliminated, both for North America (which is the same as Asia, as
 far as POWER codes are concerned -- even though much of Asia USES PAL video)
 and for Europe (which works for Australia, New Zealand, the Middle East, and
 other parts of the world that use PAL video).

 Before creating a TV-B-Gone Kit, I originally started this project by hacking
 the MiniPOV kit.  This presents a limitation, based on the size of
 the Atmel ATtiny2313 internal flash memory, which is 2KB.  With 2KB we can only
 fit about 7 POWER codes into the firmware's database of POWER codes.  However,
 the more codes the better! Which is why we chose the ATtiny85 for the
 TV-B-Gone Kit.

 This version of the firmware has the most popular 100+ POWER codes for
 North America and 100+ POWER codes for Europe. You can select which region
 to use by soldering a 10K pulldown resistor.
 */

//extern PGM_P  const *powerCodes[] PROGMEM;
//extern const struct IrCode *powerCodes[];
extern const struct PROGMEM IrCode *powerCodes[];
extern uint8_t num_codes;

extern P3310 phone;

/* This function is the 'workhorse' of transmitting IR codes.
 Given the on and off times, it turns on the PWM output on and off
 to generate one 'pair' from a long code. Each code has ~50 pairs! */
void xmitCodeElement(uint16_t ontime, uint16_t offtime, uint8_t PWM_code )
{
  TCNT2 = 0;
  if(PWM_code) {
    pinMode(IRLED, OUTPUT);
    // Fast PWM, setting top limit, divide by 8
    // Output to pin 3
    TCCR2A = _BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
    TCCR2B = _BV(WGM22) | _BV(CS21);
  }
  else {
    // However some codes dont use PWM in which case we just turn the IR
    // LED on for the period of time.
    digitalWrite(IRLED, HIGH);
  }

  // Now we wait, allowing the PWM hardware to pulse out the carrier
  // frequency for the specified 'on' time
  delay_ten_us(ontime);

  // Now we have to turn it off so disable the PWM output
  TCCR2A = 0;
  TCCR2B = 0;
  // And make sure that the IR LED is off too (since the PWM may have
  // been stopped while the LED is on!)
  digitalWrite(IRLED, LOW);

  // Now we wait for the specified 'off' time
  delay_ten_us(offtime);
}

/* This is kind of a strange but very useful helper function
 Because we are using compression, we index to the timer table
 not with a full 8-bit byte (which is wasteful) but 2 or 3 bits.
 Once code_ptr is set up to point to the right part of memory,
 this function will let us read 'count' bits at a time which
 it does by reading a byte into 'bits_r' and then buffering it. */

uint8_t bitsleft_r = 0;
uint8_t bits_r=0;
PGM_P code_ptr;

// we cant read more than 8 bits at a time so dont try!
uint8_t read_bits(uint8_t count)
{
  uint8_t i;
  uint8_t tmp=0;

  // we need to read back count bytes
  for (i=0; i<count; i++) {
    // check if the 8-bit buffer we have has run out
    if (bitsleft_r == 0) {
      // in which case we read a new byte in
      bits_r = pgm_read_byte(code_ptr++);
      // and reset the buffer size (8 bites in a byte)
      bitsleft_r = 8;
    }
    // remove one bit
    bitsleft_r--;
    // and shift it off of the end of 'bits_r'
    tmp |= (((bits_r >> (bitsleft_r)) & 1) << (count-1-i));
  }
  // return the selected bits in the LSB part of tmp
  return tmp;
}

/*
The C compiler creates code that will transfer all constants into RAM when
 the microcontroller resets.  Since this firmware has a table (powerCodes)
 that is too large to transfer into RAM, the C compiler needs to be told to
 keep it in program memory space.  This is accomplished by the macro PROGMEM
 (this is used in the definition for powerCodes).  Since the C compiler assumes
 that constants are in RAM, rather than in program memory, when accessing
 powerCodes, we need to use the pgm_read_word() and pgm_read_byte macros, and
 we need to use powerCodes as an address.  This is done with PGM_P, defined
 below.
 For example, when we start a new powerCode, we first point to it with the
 following statement:
 PGM_P thecode_p = pgm_read_word(powerCodes+i);
 The next read from the powerCode is a byte that indicates the carrier
 frequency, read as follows:
 const uint8_t freq = pgm_read_byte(code_ptr++);
 After that is a byte that tells us how many 'onTime/offTime' pairs we have:
 const uint8_t numpairs = pgm_read_byte(code_ptr++);
 The next byte tells us the compression method. Since we are going to use a
 timing table to keep track of how to pulse the LED, and the tables are
 pretty short (usually only 4-8 entries), we can index into the table with only
 2 to 4 bits. Once we know the bit-packing-size we can decode the pairs
 const uint8_t bitcompression = pgm_read_byte(code_ptr++);
 Subsequent reads from the powerCode are n bits (same as the packing size)
 that index into another table in ROM that actually stores the on/off times
 const PGM_P time_ptr = (PGM_P)pgm_read_word(code_ptr);
 */

#define FALSE 0
#define TRUE 1

void setupTVB()   {
	//Serial.begin(9600);

  TCCR2A = 0;
  TCCR2B = 0;
/*
  digitalWrite(LED, LOW);
  digitalWrite(PWMaux, LOW);
  digitalWrite(DBG, LOW);     // debug
  pinMode(LED, OUTPUT);
  pinMode(IRLED, OUTPUT);
  pinMode(DBG, OUTPUT);       // debug
  pinMode(REGIONSWITCH, INPUT);
  pinMode(TRIGGER, INPUT);
  digitalWrite(REGIONSWITCH, HIGH); //Pull-up
  digitalWrite(TRIGGER, HIGH);

  delay_ten_us(5000);            // Let everything settle for a bit
*/
  // determine region
  /*#ifdef UseEUcodes
	DEBUGP(putstring_nl("EU"));
  #else
    DEBUGP(putstring_nl("NA"));
  #endif

  // Indicate how big our database is
  DEBUGP(putstring("\n\rCodesize: ");
  putnum_ud(num_codes);*/
  
}


void Graph(uint8_t val, uint8_t maxv)
{
	//Serial.println(val);
	val = (maxv / (LCDWIDTH - 6)) * val;
	phone.lcd_buffer[(LCDWIDTH * 3) + 1] = 0xFF;
	phone.lcd_buffer[(LCDWIDTH * 3) + 82] = 0xFF;
	phone.lcd_buffer[(LCDWIDTH * 3) + 2] = 0x81;
	phone.lcd_buffer[(LCDWIDTH * 3) + 81] = 0x81;
	for(uint8_t i = 0; i < (LCDWIDTH-6); i++)
	{
		if(i < val) phone.lcd_buffer[(LCDWIDTH * 3) + i+3] = 0xBD;
		else phone.lcd_buffer[(LCDWIDTH * 3) + i+3] = 0x81;
	}
	phone.display();
}
void sendAllCodes() {

static uint16_t ontime, offtime;
static uint8_t i, Loop;
static uint8_t startOver;

/*long tm;
tm = millis();
	delay_ten_us(100);
	Serial.println(millis() - tm);
tm = millis();
delay_ten_us(1000);
Serial.println(millis() - tm);
tm = millis();
delay_ten_us(10000);
Serial.println(millis() - tm);
tm = millis();
delay_ten_us(20500);
Serial.println(millis() - tm);*/

phone.clearDisplay();
phone.LCDputsL("TVBGone",1,15);

Start_transmission:
  // startOver will become TRUE if the user pushes the Trigger button while transmitting the sequence of all codes
  startOver = FALSE;

  // for every POWER code in our collection
  for (i=0 ; i < num_codes; i++) {
    PGM_P data_ptr;
	
	Graph(i, num_codes);
    // print out the code # we are about to transmit
    //DEBUGP(putstring("\n\r\n\rCode #: ");
    //putnum_ud(i));
	//sprintf(tmpS, "Code: %d)", i);
	//phone.LCDputsL(tmpS, 0, 0);
	
    // point to next POWER code, from the right database
    data_ptr = (PGM_P)pgm_read_word(powerCodes+i);

    // print out the address in ROM memory we're reading
    //DEBUGP(putstring("\n\rAddr: ");
    //putnum_uh((uint16_t)data_ptr));

    // Read the carrier frequency from the first byte of code structure
    const uint8_t freq = pgm_read_byte(data_ptr++);
    // set OCR for Timer1 to output this POWER code's carrier frequency
    OCR2A = freq;
    OCR2B = freq / 3; // 33% duty cycle

    // Print out the frequency of the carrier and the PWM settings
    //DEBUGP(putstring("\n\rOCR1: ");
    //putnum_ud(freq);
    //);
    //DEBUGP(uint16_t x = (freq+1) * 2;
    //putstring("\n\rFreq: ");
    //putnum_ud(F_CPU/x);
    //);

    // Get the number of pairs, the second byte from the code struct
    const uint8_t numpairs = pgm_read_byte(data_ptr++);
    //DEBUGP(putstring("\n\rOn/off pairs: ");
    //putnum_ud(numpairs));

    // Get the number of bits we use to index into the timer table
    // This is the third byte of the structure
    const uint8_t bitcompression = pgm_read_byte(data_ptr++);
    //DEBUGP(putstring("\n\rCompression: ");
    //putnum_ud(bitcompression);
    //putstring("\n\r"));

    // Get pointer (address in memory) to pulse-times table
    // The address is 16-bits (2 byte, 1 word)
    PGM_P time_ptr = (PGM_P)pgm_read_word(data_ptr);
    data_ptr+=2;
    code_ptr = (PGM_P)pgm_read_word(data_ptr);

    // Transmit all codeElements for this POWER code
    // (a codeElement is an onTime and an offTime)
    // transmitting onTime means pulsing the IR emitters at the carrier
    // frequency for the length of time specified in onTime
    // transmitting offTime means no output from the IR emitters for the
    // length of time specified in offTime

/*#if 0

    // print out all of the pulse pairs
    for (uint8_t k=0; k<numpairs; k++) {
      uint8_t ti;
      ti = (read_bits(bitcompression)) * 4;
      // read the onTime and offTime from the program memory
      ontime = pgm_read_word(time_ptr+ti);
      offtime = pgm_read_word(time_ptr+ti+2);
      DEBUGP(putstring("\n\rti = ");
      putnum_ud(ti>>2);
      putstring("\tPair = ");
      putnum_ud(ontime));
      DEBUGP(putstring("\t");
      putnum_ud(offtime));
    }
    continue;
#endif*/

    // For EACH pair in this code....
	//DOESN'T WORK!! :(
	delay(100);
    /*cli();
    for (uint8_t k=0; k<numpairs; k++) {
      uint16_t ti;

      // Read the next 'n' bits as indicated by the compression variable
      // The multiply by 4 because there are 2 timing numbers per pair
      // and each timing number is one word long, so 4 bytes total!
      ti = (read_bits(bitcompression)) * 4;

      // read the onTime and offTime from the program memory
      ontime = pgm_read_word(time_ptr+ti);  // read word 1 - ontime
      offtime = pgm_read_word(time_ptr+ti+2);  // read word 2 - offtime
      // transmit this codeElement (ontime and offtime)
      xmitCodeElement(ontime, offtime, (freq!=0));
    }
    sei();*/

    //Flush remaining bits, so that next code starts
    //with a fresh set of 8 bits.
    bitsleft_r=0;

    // delay 205 milliseconds before transmitting next POWER code
    delay_ten_us(20500);

    // visible indication that a code has been output.
//    quickflashLED();

    // if user is pushing Trigger button, stop transmission
    //if (digitalRead(TRIGGER) == 0) {
	uint8_t tmpB = phone.GetBtn();
	if(tmpB == BMm){
      startOver = TRUE;
      break;
    }
	if(tmpB == BCm) return;
  }

  if (startOver) goto Start_transmission;
  while (Loop == 1);

  // flash the visible LED on PB0  8 times to indicate that we're done
  //delay_ten_us(65500); // wait maxtime
  //delay_ten_us(65500); // wait maxtime
  //quickflashLEDx(8);

}

/****************************** LED AND DELAY FUNCTIONS ********/

// This function delays the specified number of 10 microseconds
// it is 'hardcoded' and is calibrated by adjusting DELAY_CNT
// in main.h Unless you are changing the crystal from 8mhz, dont
// mess with this.
void delay_ten_us(uint16_t us) {
  uint8_t timer;
  while (us != 0) {
    // for 8MHz we want to delay 80 cycles per 10 microseconds
    // this code is tweaked to give about that amount.
    for (timer=0; timer <= DELAY_CNT; timer++) {
      NOP;
      NOP;
    }
    NOP;
    us--;
  }
}

////////////////////////////////////////////////////////////////
//			CODES!!
////////////////////////////////////////////////////////////////

//globally useful
const uint16_t code_009Times[] PROGMEM = {
	53, 56,
	53, 171,
	53, 3950,
	53, 9599,
	898, 451,
	900, 226,
};
const uint16_t code_021Times[] PROGMEM = {
	48, 52,
	48, 160,
	48, 400,
	48, 2335,
	799, 400,
};

//Codes captured from Generation 3 TV-B-Gone by Limor Fried & Mitch Altman
// table of POWER codes
#ifdef UseNAcodes
const uint16_t code_000Times[] PROGMEM = {
  60, 60,
  60, 2700,
  120, 60,
  240, 60,
};
const uint8_t code_000Codes[] PROGMEM = {
  0xE2,
  0x20,
  0x80,
  0x78,
  0x88,
  0x20,
  0x10,
};
const struct IrCode code_000Code PROGMEM = {
  freq_to_timerval(38400),
  26,             // # of pairs
  2,              // # of bits per index
  code_000Times,
  code_000Codes
};

const uint16_t code_001Times[] PROGMEM = {
  50, 100,
  50, 200,
  50, 800,
  400, 400,
};
const uint8_t code_001Codes[] PROGMEM = {
  0xD5,
  0x41,
  0x11,
  0x00,
  0x14,
  0x44,
  0x6D,
  0x54,
  0x11,
  0x10,
  0x01,
  0x44,
  0x45,
};
const struct IrCode code_001Code PROGMEM = {
  freq_to_timerval(57143),
  52,		// # of pairs
  2,		// # of bits per index
  code_001Times,
  code_001Codes
};
const uint16_t code_002Times[] PROGMEM = {
  42, 46,
  42, 133,
  42, 7519,
  347, 176,
  347, 177,
};
const uint8_t code_002Codes[] PROGMEM = {
  0x60,
  0x80,
  0x00,
  0x00,
  0x00,
  0x08,
  0x00,
  0x00,
  0x00,
  0x20,
  0x00,
  0x00,
  0x04,
  0x12,
  0x48,
  0x04,
  0x12,
  0x48,
  0x2A,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x20,
  0x00,
  0x00,
  0x00,
  0x80,
  0x00,
  0x00,
  0x10,
  0x49,
  0x20,
  0x10,
  0x49,
  0x20,
  0x80,
};
const struct IrCode code_002Code PROGMEM = {
  freq_to_timerval(37037),
  100,		// # of pairs
  3,		// # of bits per index
  code_002Times,
  code_002Codes
};
const uint16_t code_003Times[] PROGMEM = {
  26, 185,
  27, 80,
  27, 185,
  27, 4549,
};
const uint8_t code_003Codes[] PROGMEM = {
  0x15,
  0x5A,
  0x65,
  0x67,
  0x95,
  0x65,
  0x9A,
  0x9B,
  0x95,
  0x5A,
  0x65,
  0x67,
  0x95,
  0x65,
  0x9A,
  0x99,
};
const struct IrCode code_003Code PROGMEM = {
  freq_to_timerval(38610),
  64,		// # of pairs
  2,		// # of bits per index
  code_003Times,
  code_003Codes
};
const uint16_t code_004Times[] PROGMEM = {
  55, 57,
  55, 170,
  55, 3949,
  55, 9623,
  56, 0,
  898, 453,
  900, 226,
};
const uint8_t code_004Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x01,
  0x04,
  0x92,
  0x48,
  0x20,
  0x80,
  0x40,
  0x04,
  0x12,
  0x09,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_004Code PROGMEM = {
  freq_to_timerval(38610),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_004Codes
};
const uint16_t code_005Times[] PROGMEM = {
  88, 90,
  88, 91,
  88, 181,
  88, 8976,
  177, 91,
};
const uint8_t code_005Codes[] PROGMEM = {
  0x10,
  0x92,
  0x49,
  0x46,
  0x33,
  0x09,
  0x24,
  0x94,
  0x60,
};
const struct IrCode code_005Code PROGMEM = {
  freq_to_timerval(35714),
  24,		// # of pairs
  3,		// # of bits per index
  code_005Times,
  code_005Codes
};
const uint16_t code_006Times[] PROGMEM = {
  50, 62,
  50, 172,
  50, 4541,
  448, 466,
  450, 465,
};
const uint8_t code_006Codes[] PROGMEM = {
  0x64,
  0x90,
  0x00,
  0x04,
  0x90,
  0x00,
  0x00,
  0x80,
  0x00,
  0x04,
  0x12,
  0x49,
  0x2A,
  0x12,
  0x40,
  0x00,
  0x12,
  0x40,
  0x00,
  0x02,
  0x00,
  0x00,
  0x10,
  0x49,
  0x24,
  0x90,
};
const struct IrCode code_006Code PROGMEM = {
  freq_to_timerval(38462),
  68,		// # of pairs
  3,		// # of bits per index
  code_006Times,
  code_006Codes
};
const uint16_t code_007Times[] PROGMEM = {
  49, 49,
  49, 50,
  49, 410,
  49, 510,
  49, 12107,
};
const uint8_t code_007Codes[] PROGMEM = {
  0x09,
  0x94,
  0x53,
  0x29,
  0x94,
  0xD9,
  0x85,
  0x32,
  0x8A,
  0x65,
  0x32,
  0x9B,
  0x20,
};
const struct IrCode code_007Code PROGMEM = {
  freq_to_timerval(39216),
  34,		// # of pairs
  3,		// # of bits per index
  code_007Times,
  code_007Codes
};
const uint16_t code_008Times[] PROGMEM = {
  56, 58,
  56, 170,
  56, 4011,
  898, 450,
  900, 449,
};
const uint8_t code_008Codes[] PROGMEM = {
  0x64,
  0x00,
  0x49,
  0x00,
  0x92,
  0x00,
  0x20,
  0x82,
  0x01,
  0x04,
  0x10,
  0x48,
  0x2A,
  0x10,
  0x01,
  0x24,
  0x02,
  0x48,
  0x00,
  0x82,
  0x08,
  0x04,
  0x10,
  0x41,
  0x20,
  0x90,
};
const struct IrCode code_008Code PROGMEM = {
  freq_to_timerval(38462),
  68,		// # of pairs
  3,		// # of bits per index
  code_008Times,
  code_008Codes
};
const uint8_t code_009Codes[] PROGMEM = {
  0x84,
  0x90,
  0x00,
  0x20,
  0x80,
  0x08,
  0x00,
  0x00,
  0x09,
  0x24,
  0x92,
  0x40,
  0x0A,
  0xBA,
  0x40,
};
const struct IrCode code_009Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_009Codes
};
const uint16_t code_010Times[] PROGMEM = {
  51, 55,
  51, 158,
  51, 2286,
  841, 419,
};
const uint8_t code_010Codes[] PROGMEM = {
  0xD4,
  0x00,
  0x15,
  0x10,
  0x25,
  0x00,
  0x05,
  0x44,
  0x09,
  0x40,
  0x01,
  0x51,
  0x01,
};
const struct IrCode code_010Code PROGMEM = {
  freq_to_timerval(38462),
  52,		// # of pairs
  2,		// # of bits per index
  code_010Times,
  code_010Codes
};
const uint16_t code_011Times[] PROGMEM = {
  55, 55,
  55, 172,
  55, 4039,
  55, 9348,
  56, 0,
  884, 442,
  885, 225,
};
const uint8_t code_011Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x41,
  0x04,
  0x92,
  0x08,
  0x24,
  0x90,
  0x40,
  0x00,
  0x02,
  0x09,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_011Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_011Times,
  code_011Codes
};
const uint16_t code_012Times[] PROGMEM = {
  81, 87,
  81, 254,
  81, 3280,
  331, 336,
  331, 337,
};
const uint8_t code_012Codes[] PROGMEM = {
  0x64,
  0x12,
  0x08,
  0x24,
  0x00,
  0x08,
  0x20,
  0x10,
  0x09,
  0x2A,
  0x10,
  0x48,
  0x20,
  0x90,
  0x00,
  0x20,
  0x80,
  0x40,
  0x24,
  0x90,
};
const struct IrCode code_012Code PROGMEM = {
  freq_to_timerval(38462),
  52,		// # of pairs
  3,		// # of bits per index
  code_012Times,
  code_012Codes
};
const uint16_t code_013Times[] PROGMEM = {
  53, 55,
  53, 167,
  53, 2304,
  53, 9369,
  893, 448,
  895, 447,
};
const uint8_t code_013Codes[] PROGMEM = {
  0x80,
  0x12,
  0x40,
  0x04,
  0x00,
  0x09,
  0x00,
  0x12,
  0x41,
  0x24,
  0x82,
  0x01,
  0x00,
  0x10,
  0x48,
  0x24,
  0xAA,
  0xE8,
};
const struct IrCode code_013Code PROGMEM = {
  freq_to_timerval(38462),
  48,		// # of pairs
  3,		// # of bits per index
  code_013Times,
  code_013Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_014Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_014Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x09,
  0x04,
  0x92,
  0x40,
  0x24,
  0x80,
  0x00,
  0x00,
  0x12,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_014Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_014Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_015Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_015Codes[] PROGMEM = {
  0xA0,
  0x80,
  0x01,
  0x04,
  0x12,
  0x48,
  0x24,
  0x00,
  0x00,
  0x00,
  0x92,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_015Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_015Codes
};
const uint16_t code_016Times[] PROGMEM = {
  28, 90,
  28, 211,
  28, 2507,
};
const uint8_t code_016Codes[] PROGMEM = {
  0x54,
  0x04,
  0x10,
  0x00,
  0x95,
  0x01,
  0x04,
  0x00,
  0x10,
};
const struct IrCode code_016Code PROGMEM = {
  freq_to_timerval(34483),
  34,		// # of pairs
  2,		// # of bits per index
  code_016Times,
  code_016Codes
};
const uint16_t code_017Times[] PROGMEM = {
  56, 57,
  56, 175,
  56, 4150,
  56, 9499,
  898, 227,
  898, 449,
};
const uint8_t code_017Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x48,
  0x04,
  0x90,
  0x01,
  0x20,
  0x80,
  0x40,
  0x04,
  0x12,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_017Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_017Codes
};
const uint16_t code_018Times[] PROGMEM = {
  51, 55,
  51, 161,
  51, 2566,
  849, 429,
  849, 430,
};
const uint8_t code_018Codes[] PROGMEM = {
  0x60,
  0x82,
  0x08,
  0x24,
  0x10,
  0x41,
  0x00,
  0x12,
  0x40,
  0x04,
  0x80,
  0x09,
  0x2A,
  0x02,
  0x08,
  0x20,
  0x90,
  0x41,
  0x04,
  0x00,
  0x49,
  0x00,
  0x12,
  0x00,
  0x24,
  0xA8,
  0x08,
  0x20,
  0x82,
  0x41,
  0x04,
  0x10,
  0x01,
  0x24,
  0x00,
  0x48,
  0x00,
  0x92,
  0xA0,
  0x20,
  0x82,
  0x09,
  0x04,
  0x10,
  0x40,
  0x04,
  0x90,
  0x01,
  0x20,
  0x02,
  0x48,
};
const struct IrCode code_018Code PROGMEM = {
  freq_to_timerval(38462),
  136,		// # of pairs
  3,		// # of bits per index
  code_018Times,
  code_018Codes
};
const uint16_t code_019Times[] PROGMEM = {
  40, 42,
  40, 124,
  40, 4601,
  325, 163,
  326, 163,
};
const uint8_t code_019Codes[] PROGMEM = {
  0x60,
  0x10,
  0x40,
  0x04,
  0x80,
  0x09,
  0x00,
  0x00,
  0x00,
  0x00,
  0x10,
  0x00,
  0x20,
  0x10,
  0x00,
  0x20,
  0x80,
  0x00,
  0x0A,
  0x00,
  0x41,
  0x00,
  0x12,
  0x00,
  0x24,
  0x00,
  0x00,
  0x00,
  0x00,
  0x40,
  0x00,
  0x80,
  0x40,
  0x00,
  0x82,
  0x00,
  0x00,
  0x00,
};
const struct IrCode code_019Code PROGMEM = {
  freq_to_timerval(38462),
  100,		// # of pairs
  3,		// # of bits per index
  code_019Times,
  code_019Codes
};
const uint16_t code_020Times[] PROGMEM = {
  60, 55,
  60, 163,
  60, 4099,
  60, 9698,
  61, 0,
  898, 461,
  900, 230,
};
const uint8_t code_020Codes[] PROGMEM = {
  0xA0,
  0x10,
  0x00,
  0x04,
  0x82,
  0x49,
  0x20,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_020Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_020Times,
  code_020Codes
};
const uint8_t code_021Codes[] PROGMEM = {
  0x80,
  0x10,
  0x40,
  0x08,
  0x82,
  0x08,
  0x01,
  0xC0,
  0x08,
  0x20,
  0x04,
  0x41,
  0x04,
  0x00,
  0x00,
};
const struct IrCode code_021Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_021Times,
  code_021Codes
};
const uint16_t code_022Times[] PROGMEM = {
  53, 60,
  53, 175,
  53, 4463,
  53, 9453,
  892, 450,
  895, 225,
};
const uint8_t code_022Codes[] PROGMEM = {
  0x80,
  0x02,
  0x40,
  0x00,
  0x02,
  0x40,
  0x00,
  0x00,
  0x01,
  0x24,
  0x92,
  0x48,
  0x0A,
  0xBA,
  0x00,
};
const struct IrCode code_022Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_022Times,
  code_022Codes
};
const uint16_t code_023Times[] PROGMEM = {
  48, 52,
  48, 409,
  48, 504,
  48, 10461,
};
const uint8_t code_023Codes[] PROGMEM = {
  0xA1,
  0x18,
  0x61,
  0xA1,
  0x18,
  0x7A,
  0x11,
  0x86,
  0x1A,
  0x11,
  0x86,
};
const struct IrCode code_023Code PROGMEM = {
  freq_to_timerval(40000),
  44,		// # of pairs
  2,		// # of bits per index
  code_023Times,
  code_023Codes
};
const uint16_t code_024Times[] PROGMEM = {
  58, 60,
  58, 2569,
  118, 60,
  237, 60,
  238, 60,
};
const uint8_t code_024Codes[] PROGMEM = {
  0x69,
  0x24,
  0x10,
  0x40,
  0x03,
  0x12,
  0x48,
  0x20,
  0x80,
  0x00,
};
const struct IrCode code_024Code PROGMEM = {
  freq_to_timerval(38462),
  26,		// # of pairs
  3,		// # of bits per index
  code_024Times,
  code_024Codes
};
const uint16_t code_025Times[] PROGMEM = {
  84, 90,
  84, 264,
  84, 3470,
  346, 350,
  347, 350,
};
const uint8_t code_025Codes[] PROGMEM = {
  0x64,
  0x92,
  0x49,
  0x00,
  0x00,
  0x00,
  0x00,
  0x02,
  0x49,
  0x2A,
  0x12,
  0x49,
  0x24,
  0x00,
  0x00,
  0x00,
  0x00,
  0x09,
  0x24,
  0x90,
};
const struct IrCode code_025Code PROGMEM = {
  freq_to_timerval(38462),
  52,		// # of pairs
  3,		// # of bits per index
  code_025Times,
  code_025Codes
};
const uint16_t code_026Times[] PROGMEM = {
  49, 49,
  49, 50,
  49, 410,
  49, 510,
  49, 12582,
};
const uint8_t code_026Codes[] PROGMEM = {
  0x09,
  0x94,
  0x53,
  0x65,
  0x32,
  0x99,
  0x85,
  0x32,
  0x8A,
  0x6C,
  0xA6,
  0x53,
  0x20,
};
const struct IrCode code_026Code PROGMEM = {
  freq_to_timerval(39216),
  34,		// # of pairs
  3,		// # of bits per index
  code_026Times,
  code_026Codes
};

/* Duplicate timing table, same as na001 !
 const uint16_t code_027Times[] PROGMEM = {
 	50, 100,
 	50, 200,
 	50, 800,
 	400, 400,
 };
 */
const uint8_t code_027Codes[] PROGMEM = {
  0xC5,
  0x41,
  0x11,
  0x10,
  0x14,
  0x44,
  0x6C,
  0x54,
  0x11,
  0x11,
  0x01,
  0x44,
  0x44,
};
const struct IrCode code_027Code PROGMEM = {
  freq_to_timerval(57143),
  52,		// # of pairs
  2,		// # of bits per index
  code_001Times,
  code_027Codes
};
const uint16_t code_028Times[] PROGMEM = {
  118, 121,
  118, 271,
  118, 4750,
  258, 271,
};
const uint8_t code_028Codes[] PROGMEM = {
  0xC4,
  0x45,
  0x14,
  0x04,
  0x6C,
  0x44,
  0x51,
  0x40,
  0x44,
};
const struct IrCode code_028Code PROGMEM = {
  freq_to_timerval(38610),
  36,		// # of pairs
  2,		// # of bits per index
  code_028Times,
  code_028Codes
};
const uint16_t code_029Times[] PROGMEM = {
  88, 90,
  88, 91,
  88, 181,
  177, 91,
  177, 8976,
};
const uint8_t code_029Codes[] PROGMEM = {
  0x0C,
  0x92,
  0x53,
  0x46,
  0x16,
  0x49,
  0x29,
  0xA2,
  0xC0,
};
const struct IrCode code_029Code PROGMEM = {
  freq_to_timerval(35842),
  22,		// # of pairs
  3,		// # of bits per index
  code_029Times,
  code_029Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_030Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_030Codes[] PROGMEM = {
  0x80,
  0x00,
  0x41,
  0x04,
  0x12,
  0x08,
  0x20,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_030Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_030Codes
};
const uint16_t code_031Times[] PROGMEM = {
  88, 89,
  88, 90,
  88, 179,
  88, 8977,
  177, 90,
};
const uint8_t code_031Codes[] PROGMEM = {
  0x06,
  0x12,
  0x49,
  0x46,
  0x32,
  0x61,
  0x24,
  0x94,
  0x60,
};
const struct IrCode code_031Code PROGMEM = {
  freq_to_timerval(35842),
  24,		// # of pairs
  3,		// # of bits per index
  code_031Times,
  code_031Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_032Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_032Codes[] PROGMEM = {
  0x80,
  0x00,
  0x41,
  0x04,
  0x12,
  0x08,
  0x20,
  0x80,
  0x00,
  0x04,
  0x12,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_032Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_032Codes
};
const uint16_t code_033Times[] PROGMEM = {
  40, 43,
  40, 122,
  40, 5297,
  334, 156,
  336, 155,
};
const uint8_t code_033Codes[] PROGMEM = {
  0x60,
  0x10,
  0x40,
  0x04,
  0x80,
  0x09,
  0x00,
  0x00,
  0x00,
  0x00,
  0x10,
  0x00,
  0x20,
  0x82,
  0x00,
  0x20,
  0x00,
  0x00,
  0x0A,
  0x00,
  0x41,
  0x00,
  0x12,
  0x00,
  0x24,
  0x00,
  0x00,
  0x00,
  0x00,
  0x40,
  0x00,
  0x82,
  0x08,
  0x00,
  0x80,
  0x00,
  0x00,
  0x00,
};
const struct IrCode code_033Code PROGMEM = {
  freq_to_timerval(38462),
  100,		// # of pairs
  3,		// # of bits per index
  code_033Times,
  code_033Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_034Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_034Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x41,
  0x04,
  0x92,
  0x08,
  0x24,
  0x92,
  0x48,
  0x00,
  0x00,
  0x01,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_034Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_034Codes
};
const uint16_t code_035Times[] PROGMEM = {
  96, 93,
  97, 93,
  97, 287,
  97, 3431,
};
const uint8_t code_035Codes[] PROGMEM = {
  0x16,
  0x66,
  0x5D,
  0x59,
  0x99,
  0x50,
};
const struct IrCode code_035Code PROGMEM = {
  freq_to_timerval(41667),
  22,		// # of pairs
  2,		// # of bits per index
  code_035Times,
  code_035Codes
};
const uint16_t code_036Times[] PROGMEM = {
  82, 581,
  84, 250,
  84, 580,
  85, 0,
};
const uint8_t code_036Codes[] PROGMEM = {
  0x15,
  0x9A,
  0x9C,
};
const struct IrCode code_036Code PROGMEM = {
  freq_to_timerval(37037),
  11,		// # of pairs
  2,		// # of bits per index
  code_036Times,
  code_036Codes
};
const uint16_t code_037Times[] PROGMEM = {
  39, 263,
  164, 163,
  514, 164,
};
const uint8_t code_037Codes[] PROGMEM = {
  0x80,
  0x45,
  0x00,
};
const struct IrCode code_037Code PROGMEM = {
  freq_to_timerval(41667),
  11,		// # of pairs
  2,		// # of bits per index
  code_037Times,
  code_037Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_038Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_038Codes[] PROGMEM = {
  0xA4,
  0x10,
  0x40,
  0x00,
  0x82,
  0x09,
  0x20,
  0x80,
  0x40,
  0x04,
  0x12,
  0x09,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_038Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_038Codes
};
const uint16_t code_039Times[] PROGMEM = {
  113, 101,
  688, 2707,
};
const uint8_t code_039Codes[] PROGMEM = {
  0x11,
};
const struct IrCode code_039Code PROGMEM = {
  freq_to_timerval(40000),
  4,		// # of pairs
  2,		// # of bits per index
  code_039Times,
  code_039Codes
};
const uint16_t code_040Times[] PROGMEM = {
  113, 101,
  113, 201,
  113, 2707,
};
const uint8_t code_040Codes[] PROGMEM = {
  0x06,
  0x04,
};
const struct IrCode code_040Code PROGMEM = {
  freq_to_timerval(40000),
  8,		// # of pairs
  2,		// # of bits per index
  code_040Times,
  code_040Codes
};
const uint16_t code_041Times[] PROGMEM = {
  58, 62,
  58, 2746,
  117, 62,
  242, 62,
};
const uint8_t code_041Codes[] PROGMEM = {
  0xE2,
  0x20,
  0x80,
  0x78,
  0x88,
  0x20,
  0x00,
};
const struct IrCode code_041Code PROGMEM = {
  freq_to_timerval(76923),
  26,		// # of pairs
  2,		// # of bits per index
  code_041Times,
  code_041Codes
};
const uint16_t code_042Times[] PROGMEM = {
  54, 65,
  54, 170,
  54, 4099,
  54, 8668,
  899, 226,
  899, 421,
};
const uint8_t code_042Codes[] PROGMEM = {
  0xA4,
  0x80,
  0x00,
  0x20,
  0x82,
  0x49,
  0x00,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_042Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_042Times,
  code_042Codes
};
const uint16_t code_043Times[] PROGMEM = {
  43, 120,
  43, 121,
  43, 3491,
  131, 45,
};
const uint8_t code_043Codes[] PROGMEM = {
  0x15,
  0x75,
  0x56,
  0x55,
  0x75,
  0x54,
};
const struct IrCode code_043Code PROGMEM = {
  freq_to_timerval(40000),
  24,		// # of pairs
  2,		// # of bits per index
  code_043Times,
  code_043Codes
};
const uint16_t code_044Times[] PROGMEM = {
  51, 51,
  51, 160,
  51, 4096,
  51, 9513,
  431, 436,
  883, 219,
};
const uint8_t code_044Codes[] PROGMEM = {
  0x84,
  0x90,
  0x00,
  0x00,
  0x02,
  0x49,
  0x20,
  0x80,
  0x00,
  0x04,
  0x12,
  0x49,
  0x2A,
  0xBA,
  0x40,
};
const struct IrCode code_044Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_044Times,
  code_044Codes
};
const uint16_t code_045Times[] PROGMEM = {
  58, 53,
  58, 167,
  58, 4494,
  58, 9679,
  455, 449,
  456, 449,
};
const uint8_t code_045Codes[] PROGMEM = {
  0x80,
  0x90,
  0x00,
  0x00,
  0x90,
  0x00,
  0x04,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0x97,
  0x48,
};
const struct IrCode code_045Code PROGMEM = {
  freq_to_timerval(38462),
  40,		// # of pairs
  3,		// # of bits per index
  code_045Times,
  code_045Codes
};
const uint16_t code_046Times[] PROGMEM = {
  51, 277,
  52, 53,
  52, 105,
  52, 277,
  52, 2527,
  52, 12809,
  103, 54,
};
const uint8_t code_046Codes[] PROGMEM = {
  0x0B,
  0x12,
  0x63,
  0x44,
  0x92,
  0x6B,
  0x44,
  0x92,
  0x50,
};
const struct IrCode code_046Code PROGMEM = {
  freq_to_timerval(29412),
  23,		// # of pairs
  3,		// # of bits per index
  code_046Times,
  code_046Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_047Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_047Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x40,
  0x04,
  0x92,
  0x09,
  0x24,
  0x92,
  0x09,
  0x20,
  0x00,
  0x40,
  0x0A,
  0x38,
  0x00,
};
const struct IrCode code_047Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_047Codes
};

/* Duplicate timing table, same as na044 !
 const uint16_t code_048Times[] PROGMEM = {
 	51, 51,
 	51, 160,
 	51, 4096,
 	51, 9513,
 	431, 436,
 	883, 219,
 };
 */
const uint8_t code_048Codes[] PROGMEM = {
  0x80,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x24,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_048Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_044Times,
  code_048Codes
};
const uint16_t code_049Times[] PROGMEM = {
  274, 854,
  274, 1986,
};
const uint8_t code_049Codes[] PROGMEM = {
  0x14,
  0x11,
  0x40,
};
const struct IrCode code_049Code PROGMEM = {
  freq_to_timerval(45455),
  11,		// # of pairs
  2,		// # of bits per index
  code_049Times,
  code_049Codes
};
const uint16_t code_050Times[] PROGMEM = {
  80, 88,
  80, 254,
  80, 3750,
  359, 331,
};
const uint8_t code_050Codes[] PROGMEM = {
  0xC0,
  0x00,
  0x01,
  0x55,
  0x55,
  0x52,
  0xC0,
  0x00,
  0x01,
  0x55,
  0x55,
  0x50,
};
const struct IrCode code_050Code PROGMEM = {
  freq_to_timerval(55556),
  48,		// # of pairs
  2,		// # of bits per index
  code_050Times,
  code_050Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_051Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_051Codes[] PROGMEM = {
  0xA0,
  0x10,
  0x01,
  0x24,
  0x82,
  0x48,
  0x00,
  0x02,
  0x40,
  0x04,
  0x90,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_051Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_051Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_052Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_052Codes[] PROGMEM = {
  0xA4,
  0x90,
  0x48,
  0x00,
  0x02,
  0x01,
  0x20,
  0x80,
  0x40,
  0x04,
  0x12,
  0x09,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_052Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_052Codes
};
const uint16_t code_053Times[] PROGMEM = {
  51, 232,
  51, 512,
  51, 792,
  51, 2883,
};
const uint8_t code_053Codes[] PROGMEM = {
  0x22,
  0x21,
  0x40,
  0x1C,
  0x88,
  0x85,
  0x00,
  0x40,
};
const struct IrCode code_053Code PROGMEM = {
  freq_to_timerval(55556),
  30,		// # of pairs
  2,		// # of bits per index
  code_053Times,
  code_053Codes
};

/* Duplicate timing table, same as na053 !
 const uint16_t code_054Times[] PROGMEM = {
 	51, 232,
 	51, 512,
 	51, 792,
 	51, 2883,
 };
 */
const uint8_t code_054Codes[] PROGMEM = {
  0x22,
  0x20,
  0x15,
  0x72,
  0x22,
  0x01,
  0x54,
};
const struct IrCode code_054Code PROGMEM = {
  freq_to_timerval(55556),
  28,		// # of pairs
  2,		// # of bits per index
  code_053Times,
  code_054Codes
};
const uint16_t code_055Times[] PROGMEM = {
  3, 10,
  3, 20,
  3, 30,
  3, 12778,
};
const uint8_t code_055Codes[] PROGMEM = {
  0x81,
  0x51,
  0x14,
  0xB8,
  0x15,
  0x11,
  0x44,
};
const struct IrCode code_055Code PROGMEM = {
  0,              // Non-pulsed code
  27,		// # of pairs
  2,		// # of bits per index
  code_055Times,
  code_055Codes
};
const uint16_t code_056Times[] PROGMEM = {
  55, 193,
  57, 192,
  57, 384,
  58, 0,
};
const uint8_t code_056Codes[] PROGMEM = {
  0x2A,
  0x57,
};
const struct IrCode code_056Code PROGMEM = {
  freq_to_timerval(37175),
  8,		// # of pairs
  2,		// # of bits per index
  code_056Times,
  code_056Codes
};
const uint16_t code_057Times[] PROGMEM = {
  45, 148,
  46, 148,
  46, 351,
  46, 2781,
};
const uint8_t code_057Codes[] PROGMEM = {
  0x2A,
  0x5D,
  0xA9,
  0x60,
};
const struct IrCode code_057Code PROGMEM = {
  freq_to_timerval(40000),
  14,		// # of pairs
  2,		// # of bits per index
  code_057Times,
  code_057Codes
};
const uint16_t code_058Times[] PROGMEM = {
  22, 101,
  22, 219,
  23, 101,
  23, 219,
  31, 218,
};
const uint8_t code_058Codes[] PROGMEM = {
  0x8D,
  0xA4,
  0x08,
  0x04,
  0x04,
  0x92,
  0x4C,
};
const struct IrCode code_058Code PROGMEM = {
  freq_to_timerval(33333),
  18,		// # of pairs
  3,		// # of bits per index
  code_058Times,
  code_058Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_059Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_059Codes[] PROGMEM = {
  0xA4,
  0x12,
  0x09,
  0x00,
  0x80,
  0x40,
  0x20,
  0x10,
  0x40,
  0x04,
  0x82,
  0x09,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_059Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_059Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_060Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_060Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x08,
  0x04,
  0x92,
  0x41,
  0x24,
  0x00,
  0x40,
  0x00,
  0x92,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_060Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_060Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_061Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_061Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x08,
  0x24,
  0x92,
  0x41,
  0x04,
  0x82,
  0x00,
  0x00,
  0x10,
  0x49,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_061Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_061Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_062Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_062Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x08,
  0x04,
  0x90,
  0x41,
  0x24,
  0x82,
  0x00,
  0x00,
  0x10,
  0x49,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_062Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_062Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_063Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_063Codes[] PROGMEM = {
  0xA4,
  0x92,
  0x49,
  0x20,
  0x00,
  0x00,
  0x04,
  0x92,
  0x48,
  0x00,
  0x00,
  0x01,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_063Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_063Codes
};

/* Duplicate timing table, same as na001 !
 const uint16_t code_064Times[] PROGMEM = {
 	50, 100,
 	50, 200,
 	50, 800,
 	400, 400,
 };
 */
const uint8_t code_064Codes[] PROGMEM = {
  0xC0,
  0x01,
  0x51,
  0x55,
  0x54,
  0x04,
  0x2C,
  0x00,
  0x15,
  0x15,
  0x55,
  0x40,
  0x40,
};
const struct IrCode code_064Code PROGMEM = {
  freq_to_timerval(57143),
  52,		// # of pairs
  2,		// # of bits per index
  code_001Times,
  code_064Codes
};
const uint16_t code_065Times[] PROGMEM = {
  48, 98,
  48, 197,
  98, 846,
  395, 392,
  1953, 392,
};
const uint8_t code_065Codes[] PROGMEM = {
  0x84,
  0x92,
  0x01,
  0x24,
  0x12,
  0x00,
  0x04,
  0x80,
  0x08,
  0x09,
  0x92,
  0x48,
  0x04,
  0x90,
  0x48,
  0x00,
  0x12,
  0x00,
  0x20,
  0x26,
  0x49,
  0x20,
  0x12,
  0x41,
  0x20,
  0x00,
  0x48,
  0x00,
  0x80,
  0x80,
};
const struct IrCode code_065Code PROGMEM = {
  freq_to_timerval(59172),
  78,		// # of pairs
  3,		// # of bits per index
  code_065Times,
  code_065Codes
};
const uint16_t code_066Times[] PROGMEM = {
  38, 276,
  165, 154,
  415, 155,
  742, 154,
};
const uint8_t code_066Codes[] PROGMEM = {
  0xC0,
  0x45,
  0x02,
  0x01,
  0x14,
  0x08,
  0x04,
  0x50,
  0x00,
};
const struct IrCode code_066Code PROGMEM = {
  freq_to_timerval(38462),
  33,		// # of pairs
  2,		// # of bits per index
  code_066Times,
  code_066Codes
};

/* Duplicate timing table, same as na044 !
 const uint16_t code_067Times[] PROGMEM = {
 	51, 51,
 	51, 160,
 	51, 4096,
 	51, 9513,
 	431, 436,
 	883, 219,
 };
 */
const uint8_t code_067Codes[] PROGMEM = {
  0x80,
  0x02,
  0x49,
  0x24,
  0x90,
  0x00,
  0x00,
  0x80,
  0x00,
  0x04,
  0x12,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_067Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_044Times,
  code_067Codes
};
const uint16_t code_068Times[] PROGMEM = {
  43, 121,
  43, 9437,
  130, 45,
  131, 45,
};
const uint8_t code_068Codes[] PROGMEM = {
  0x8C,
  0x30,
  0x0D,
  0xCC,
  0x30,
  0x0C,
};
const struct IrCode code_068Code PROGMEM = {
  freq_to_timerval(40000),
  24,		// # of pairs
  2,		// # of bits per index
  code_068Times,
  code_068Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_069Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_069Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x24,
  0x82,
  0x00,
  0x00,
  0x10,
  0x49,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_069Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_069Codes
};
const uint16_t code_070Times[] PROGMEM = {
  27, 76,
  27, 182,
  27, 183,
  27, 3199,
};
const uint8_t code_070Codes[] PROGMEM = {
  0x40,
  0x02,
  0x08,
  0xA2,
  0xE0,
  0x00,
  0x82,
  0x28,
  0x40,
};
const struct IrCode code_070Code PROGMEM = {
  freq_to_timerval(38462),
  33,		// # of pairs
  2,		// # of bits per index
  code_070Times,
  code_070Codes
};
const uint16_t code_071Times[] PROGMEM = {
  37, 181,
  37, 272,
};
const uint8_t code_071Codes[] PROGMEM = {
  0x11,
  0x40,
};
const struct IrCode code_071Code PROGMEM = {
  freq_to_timerval(55556),
  8,		// # of pairs
  2,		// # of bits per index
  code_071Times,
  code_071Codes
};

/* Duplicate timing table, same as na042 !
 const uint16_t code_072Times[] PROGMEM = {
 	54, 65,
 	54, 170,
 	54, 4099,
 	54, 8668,
 	899, 226,
 	899, 421,
 };
 */
const uint8_t code_072Codes[] PROGMEM = {
  0xA0,
  0x90,
  0x00,
  0x00,
  0x90,
  0x00,
  0x00,
  0x10,
  0x40,
  0x04,
  0x82,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_072Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_042Times,
  code_072Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_073Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_073Codes[] PROGMEM = {
  0xA0,
  0x82,
  0x08,
  0x24,
  0x10,
  0x41,
  0x00,
  0x00,
  0x00,
  0x24,
  0x92,
  0x49,
  0x0A,
  0x38,
  0x00,
};
const struct IrCode code_073Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_073Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_074Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_074Codes[] PROGMEM = {
  0xA4,
  0x00,
  0x41,
  0x00,
  0x92,
  0x08,
  0x20,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_074Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_074Codes
};
const uint16_t code_075Times[] PROGMEM = {
  51, 98,
  51, 194,
  102, 931,
  390, 390,
  390, 391,
};
const uint8_t code_075Codes[] PROGMEM = {
  0x60,
  0x00,
  0x01,
  0x04,
  0x10,
  0x49,
  0x24,
  0x82,
  0x08,
  0x2A,
  0x00,
  0x00,
  0x04,
  0x10,
  0x41,
  0x24,
  0x92,
  0x08,
  0x20,
  0xA0,
};
const struct IrCode code_075Code PROGMEM = {
  freq_to_timerval(41667),
  52,		// # of pairs
  3,		// # of bits per index
  code_075Times,
  code_075Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_076Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_076Codes[] PROGMEM = {
  0xA0,
  0x92,
  0x09,
  0x04,
  0x00,
  0x40,
  0x20,
  0x10,
  0x40,
  0x04,
  0x82,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_076Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_076Codes
};

/* Duplicate timing table, same as na031 !
 const uint16_t code_077Times[] PROGMEM = {
 	88, 89,
 	88, 90,
 	88, 179,
 	88, 8977,
 	177, 90,
 };
 */
const uint8_t code_077Codes[] PROGMEM = {
  0x10,
  0xA2,
  0x62,
  0x31,
  0x98,
  0x51,
  0x31,
  0x18,
  0x00,
};
const struct IrCode code_077Code PROGMEM = {
  freq_to_timerval(35714),
  22,		// # of pairs
  3,		// # of bits per index
  code_031Times,
  code_077Codes
};
const uint16_t code_078Times[] PROGMEM = {
  40, 275,
  160, 154,
  480, 155,
};
const uint8_t code_078Codes[] PROGMEM = {
  0x80,
  0x45,
  0x04,
  0x01,
  0x14,
  0x10,
  0x04,
  0x50,
  0x40,
};
const struct IrCode code_078Code PROGMEM = {
  freq_to_timerval(38462),
  34,		// # of pairs
  2,		// # of bits per index
  code_078Times,
  code_078Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_079Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_079Codes[] PROGMEM = {
  0xA0,
  0x82,
  0x08,
  0x24,
  0x10,
  0x41,
  0x04,
  0x90,
  0x08,
  0x20,
  0x02,
  0x41,
  0x0A,
  0x38,
  0x00,
};
const struct IrCode code_079Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_079Codes
};

/* Duplicate timing table, same as na055 !
 const uint16_t code_080Times[] PROGMEM = {
 	3, 10,
 	3, 20,
 	3, 30,
 	3, 12778,
 };
 */
const uint8_t code_080Codes[] PROGMEM = {
  0x81,
  0x50,
  0x40,
  0xB8,
  0x15,
  0x04,
  0x08,
};
const struct IrCode code_080Code PROGMEM = {
  0,              // Non-pulsed code
  27,		// # of pairs
  2,		// # of bits per index
  code_055Times,
  code_080Codes
};
const uint16_t code_081Times[] PROGMEM = {
  48, 52,
  48, 409,
  48, 504,
  48, 9978,
};
const uint8_t code_081Codes[] PROGMEM = {
  0x18,
  0x46,
  0x18,
  0x68,
  0x47,
  0x18,
  0x46,
  0x18,
  0x68,
  0x44,
};
const struct IrCode code_081Code PROGMEM = {
  freq_to_timerval(40000),
  40,		// # of pairs
  2,		// # of bits per index
  code_081Times,
  code_081Codes
};
const uint16_t code_082Times[] PROGMEM = {
  88, 89,
  88, 90,
  88, 179,
  88, 8888,
  177, 90,
  177, 179,
};
const uint8_t code_082Codes[] PROGMEM = {
  0x0A,
  0x12,
  0x49,
  0x2A,
  0xB2,
  0xA1,
  0x24,
  0x92,
  0xA8,
};
const struct IrCode code_082Code PROGMEM = {
  freq_to_timerval(35714),
  24,		// # of pairs
  3,		// # of bits per index
  code_082Times,
  code_082Codes
};

/* Duplicate timing table, same as na031 !
 const uint16_t code_083Times[] PROGMEM = {
 	88, 89,
 	88, 90,
 	88, 179,
 	88, 8977,
 	177, 90,
 };
 */
const uint8_t code_083Codes[] PROGMEM = {
  0x10,
  0x92,
  0x49,
  0x46,
  0x33,
  0x09,
  0x24,
  0x94,
  0x60,
};
const struct IrCode code_083Code PROGMEM = {
  freq_to_timerval(35714),
  24,		// # of pairs
  3,		// # of bits per index
  code_031Times,
  code_083Codes
};

const uint16_t code_084Times[] PROGMEM = {
  41, 43,
  41, 128,
  41, 7476,
  336, 171,
  338, 169,
};
const uint8_t code_084Codes[] PROGMEM = {
  0x60,
  0x80,
  0x00,
  0x00,
  0x00,
  0x08,
  0x00,
  0x00,
  0x40,
  0x20,
  0x00,
  0x00,
  0x04,
  0x12,
  0x48,
  0x04,
  0x12,
  0x08,
  0x2A,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x20,
  0x00,
  0x01,
  0x00,
  0x80,
  0x00,
  0x00,
  0x10,
  0x49,
  0x20,
  0x10,
  0x48,
  0x20,
  0x80,
};
const struct IrCode code_084Code PROGMEM = {
  freq_to_timerval(37037),
  100,		// # of pairs
  3,		// # of bits per index
  code_084Times,
  code_084Codes
};
const uint16_t code_085Times[] PROGMEM = {
  55, 60,
  55, 165,
  55, 2284,
  445, 437,
  448, 436,
};
const uint8_t code_085Codes[] PROGMEM = {
  0x64,
  0x00,
  0x00,
  0x00,
  0x00,
  0x40,
  0x00,
  0x80,
  0xA1,
  0x00,
  0x00,
  0x00,
  0x00,
  0x10,
  0x00,
  0x20,
  0x10,
};
const struct IrCode code_085Code PROGMEM = {
  freq_to_timerval(38462),
  44,		// # of pairs
  3,		// # of bits per index
  code_085Times,
  code_085Codes
};
const uint16_t code_086Times[] PROGMEM = {
  42, 46,
  42, 126,
  42, 6989,
  347, 176,
  347, 177,
};
const uint8_t code_086Codes[] PROGMEM = {
  0x60,
  0x82,
  0x08,
  0x20,
  0x82,
  0x41,
  0x04,
  0x92,
  0x00,
  0x20,
  0x80,
  0x40,
  0x00,
  0x90,
  0x40,
  0x04,
  0x00,
  0x41,
  0x2A,
  0x02,
  0x08,
  0x20,
  0x82,
  0x09,
  0x04,
  0x12,
  0x48,
  0x00,
  0x82,
  0x01,
  0x00,
  0x02,
  0x41,
  0x00,
  0x10,
  0x01,
  0x04,
  0x80,
};
const struct IrCode code_086Code PROGMEM = {
  freq_to_timerval(37175),
  100,		// # of pairs
  3,		// # of bits per index
  code_086Times,
  code_086Codes
};
const uint16_t code_087Times[] PROGMEM = {
  56, 69,
  56, 174,
  56, 4165,
  56, 9585,
  880, 222,
  880, 435,
};
const uint8_t code_087Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x40,
  0x04,
  0x90,
  0x09,
  0x20,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_087Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_087Times,
  code_087Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_088Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_088Codes[] PROGMEM = {
  0x80,
  0x00,
  0x40,
  0x04,
  0x12,
  0x08,
  0x04,
  0x92,
  0x40,
  0x00,
  0x00,
  0x09,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_088Code PROGMEM = {
  freq_to_timerval(38610),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_088Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_089Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_089Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x20,
  0x80,
  0x40,
  0x04,
  0x12,
  0x09,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_089Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_089Codes
};
const uint16_t code_090Times[] PROGMEM = {
  88, 90,
  88, 91,
  88, 181,
  88, 8976,
  177, 91,
  177, 181,
};
const uint8_t code_090Codes[] PROGMEM = {
  0x10,
  0xAB,
  0x11,
  0x8C,
  0xC2,
  0xAC,
  0x46,
  0x00,
};
const struct IrCode code_090Code PROGMEM = {
  freq_to_timerval(35714),
  20,		// # of pairs
  3,		// # of bits per index
  code_090Times,
  code_090Codes
};
const uint16_t code_091Times[] PROGMEM = {
  48, 100,
  48, 200,
  48, 1050,
  400, 400,
};
const uint8_t code_091Codes[] PROGMEM = {
  0xD5,
  0x41,
  0x51,
  0x40,
  0x14,
  0x04,
  0x2D,
  0x54,
  0x15,
  0x14,
  0x01,
  0x40,
  0x41,
};
const struct IrCode code_091Code PROGMEM = {
  freq_to_timerval(58824),
  52,		// # of pairs
  2,		// # of bits per index
  code_091Times,
  code_091Codes
};
const uint16_t code_092Times[] PROGMEM = {
  54, 56,
  54, 170,
  54, 4927,
  451, 447,
};
const uint8_t code_092Codes[] PROGMEM = {
  0xD1,
  0x00,
  0x11,
  0x00,
  0x04,
  0x00,
  0x11,
  0x55,
  0x6D,
  0x10,
  0x01,
  0x10,
  0x00,
  0x40,
  0x01,
  0x15,
  0x55,
};
const struct IrCode code_092Code PROGMEM = {
  freq_to_timerval(38462),
  68,		// # of pairs
  2,		// # of bits per index
  code_092Times,
  code_092Codes
};
const uint16_t code_093Times[] PROGMEM = {
  55, 57,
  55, 167,
  55, 4400,
  895, 448,
  897, 447,
};
const uint8_t code_093Codes[] PROGMEM = {
  0x60,
  0x90,
  0x00,
  0x20,
  0x80,
  0x00,
  0x04,
  0x02,
  0x01,
  0x00,
  0x90,
  0x48,
  0x2A,
  0x02,
  0x40,
  0x00,
  0x82,
  0x00,
  0x00,
  0x10,
  0x08,
  0x04,
  0x02,
  0x41,
  0x20,
  0x80,
};
const struct IrCode code_093Code PROGMEM = {
  freq_to_timerval(38462),
  68,		// # of pairs
  3,		// # of bits per index
  code_093Times,
  code_093Codes
};

/* Duplicate timing table, same as na005 !
 const uint16_t code_094Times[] PROGMEM = {
 	88, 90,
 	88, 91,
 	88, 181,
 	88, 8976,
 	177, 91,
 };
 */
const uint8_t code_094Codes[] PROGMEM = {
  0x10,
  0x94,
  0x62,
  0x31,
  0x98,
  0x4A,
  0x31,
  0x18,
  0x00,
};
const struct IrCode code_094Code PROGMEM = {
  freq_to_timerval(35714),
  22,		// # of pairs
  3,		// # of bits per index
  code_005Times,
  code_094Codes
};
const uint16_t code_095Times[] PROGMEM = {
  56, 58,
  56, 174,
  56, 4549,
  56, 9448,
  440, 446,
};
const uint8_t code_095Codes[] PROGMEM = {
  0x80,
  0x02,
  0x00,
  0x00,
  0x02,
  0x00,
  0x04,
  0x82,
  0x00,
  0x00,
  0x10,
  0x49,
  0x2A,
  0x17,
  0x08,
};
const struct IrCode code_095Code PROGMEM = {
  freq_to_timerval(38462),
  40,		// # of pairs
  3,		// # of bits per index
  code_095Times,
  code_095Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_096Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_096Codes[] PROGMEM = {
  0x80,
  0x80,
  0x40,
  0x04,
  0x92,
  0x49,
  0x20,
  0x92,
  0x00,
  0x04,
  0x00,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_096Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_096Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_097Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_097Codes[] PROGMEM = {
  0x84,
  0x80,
  0x00,
  0x24,
  0x10,
  0x41,
  0x00,
  0x80,
  0x01,
  0x24,
  0x12,
  0x48,
  0x0A,
  0xBA,
  0x40,
};
const struct IrCode code_097Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_097Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_098Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_098Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x24,
  0x00,
  0x41,
  0x00,
  0x92,
  0x08,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_098Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_098Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_099Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_099Codes[] PROGMEM = {
  0x80,
  0x00,
  0x00,
  0x04,
  0x12,
  0x48,
  0x24,
  0x00,
  0x00,
  0x00,
  0x92,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_099Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_099Codes
};
const uint16_t code_100Times[] PROGMEM = {
  43, 171,
  45, 60,
  45, 170,
  54, 2301,
};
const uint8_t code_100Codes[] PROGMEM = {
  0x29,
  0x59,
  0x65,
  0x55,
  0xEA,
  0x56,
  0x59,
  0x55,
  0x70,
};
const struct IrCode code_100Code PROGMEM = {
  freq_to_timerval(35842),
  34,		// # of pairs
  2,		// # of bits per index
  code_100Times,
  code_100Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_101Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_101Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x09,
  0x04,
  0x92,
  0x40,
  0x20,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_101Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_101Codes
};
const uint16_t code_102Times[] PROGMEM = {
  86, 87,
  86, 258,
  86, 3338,
  346, 348,
  348, 347,
};
const uint8_t code_102Codes[] PROGMEM = {
  0x64,
  0x02,
  0x08,
  0x00,
  0x02,
  0x09,
  0x04,
  0x12,
  0x49,
  0x0A,
  0x10,
  0x08,
  0x20,
  0x00,
  0x08,
  0x24,
  0x10,
  0x49,
  0x24,
  0x10,
};
const struct IrCode code_102Code PROGMEM = {
  freq_to_timerval(40000),
  52,		// # of pairs
  3,		// # of bits per index
  code_102Times,
  code_102Codes
};

/* Duplicate timing table, same as na045 !
 const uint16_t code_103Times[] PROGMEM = {
 	58, 53,
 	58, 167,
 	58, 4494,
 	58, 9679,
 	455, 449,
 	456, 449,
 };
 */
const uint8_t code_103Codes[] PROGMEM = {
  0x80,
  0x02,
  0x00,
  0x00,
  0x02,
  0x00,
  0x04,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0x97,
  0x48,
};
const struct IrCode code_103Code PROGMEM = {
  freq_to_timerval(38462),
  40,		// # of pairs
  3,		// # of bits per index
  code_045Times,
  code_103Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_104Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_104Codes[] PROGMEM = {
  0xA4,
  0x00,
  0x49,
  0x00,
  0x92,
  0x00,
  0x20,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_104Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_104Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_105Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_105Codes[] PROGMEM = {
  0xA4,
  0x80,
  0x00,
  0x20,
  0x12,
  0x49,
  0x04,
  0x92,
  0x49,
  0x20,
  0x00,
  0x00,
  0x0A,
  0x38,
  0x40,
};
const struct IrCode code_105Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_105Codes
};

/* Duplicate timing table, same as na044 !
 const uint16_t code_106Times[] PROGMEM = {
 	51, 51,
 	51, 160,
 	51, 4096,
 	51, 9513,
 	431, 436,
 	883, 219,
 };
 */
const uint8_t code_106Codes[] PROGMEM = {
  0x80,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x24,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_106Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_044Times,
  code_106Codes
};

/* Duplicate timing table, same as na045 !
 const uint16_t code_107Times[] PROGMEM = {
 	58, 53,
 	58, 167,
 	58, 4494,
 	58, 9679,
 	455, 449,
 	456, 449,
 };
 */
const uint8_t code_107Codes[] PROGMEM = {
  0x80,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0x97,
  0x48,
};
const struct IrCode code_107Code PROGMEM = {
  freq_to_timerval(38462),
  40,		// # of pairs
  3,		// # of bits per index
  code_045Times,
  code_107Codes
};

/* Duplicate timing table, same as na045 !
 const uint16_t code_108Times[] PROGMEM = {
 	58, 53,
 	58, 167,
 	58, 4494,
 	58, 9679,
 	455, 449,
 	456, 449,
 };
 */
const uint8_t code_108Codes[] PROGMEM = {
  0x80,
  0x90,
  0x40,
  0x00,
  0x90,
  0x40,
  0x04,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0x97,
  0x48,
};
const struct IrCode code_108Code PROGMEM = {
  freq_to_timerval(38462),
  40,		// # of pairs
  3,		// # of bits per index
  code_045Times,
  code_108Codes
};
const uint16_t code_109Times[] PROGMEM = {
  58, 61,
  58, 211,
  58, 9582,
  73, 4164,
  883, 211,
  1050, 494,
};
const uint8_t code_109Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x08,
  0x24,
  0x92,
  0x41,
  0x00,
  0x82,
  0x00,
  0x04,
  0x10,
  0x49,
  0x2E,
  0x28,
  0x00,
};
const struct IrCode code_109Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_109Times,
  code_109Codes
};


/* Duplicate timing table, same as na017 !
 const uint16_t code_110Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_110Codes[] PROGMEM = {
  0xA4,
  0x80,
  0x00,
  0x20,
  0x12,
  0x49,
  0x00,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_110Code PROGMEM = {
  freq_to_timerval(40161),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_110Codes
};

/* Duplicate timing table, same as na044 !
 const uint16_t code_111Times[] PROGMEM = {
 	51, 51,
 	51, 160,
 	51, 4096,
 	51, 9513,
 	431, 436,
 	883, 219,
 };
 */
const uint8_t code_111Codes[] PROGMEM = {
  0x84,
  0x92,
  0x49,
  0x20,
  0x00,
  0x00,
  0x04,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0xBA,
  0x40,
};
const struct IrCode code_111Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_044Times,
  code_111Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_112Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_112Codes[] PROGMEM = {
  0xA4,
  0x00,
  0x00,
  0x00,
  0x92,
  0x49,
  0x24,
  0x00,
  0x00,
  0x00,
  0x92,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_112Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_112Codes
};
const uint16_t code_113Times[] PROGMEM = {
  56, 54,
  56, 166,
  56, 3945,
  896, 442,
  896, 443,
};
const uint8_t code_113Codes[] PROGMEM = {
  0x60,
  0x00,
  0x00,
  0x20,
  0x02,
  0x09,
  0x04,
  0x02,
  0x01,
  0x00,
  0x90,
  0x48,
  0x2A,
  0x00,
  0x00,
  0x00,
  0x80,
  0x08,
  0x24,
  0x10,
  0x08,
  0x04,
  0x02,
  0x41,
  0x20,
  0x80,
};
const struct IrCode code_113Code PROGMEM = {
  freq_to_timerval(40000),
  68,		// # of pairs
  3,		// # of bits per index
  code_113Times,
  code_113Codes
};
const uint16_t code_114Times[] PROGMEM = {
  44, 50,
  44, 147,
  44, 447,
  44, 2236,
  791, 398,
  793, 397,
};
const uint8_t code_114Codes[] PROGMEM = {
  0x84,
  0x10,
  0x40,
  0x08,
  0x82,
  0x08,
  0x01,
  0xD2,
  0x08,
  0x20,
  0x04,
  0x41,
  0x04,
  0x00,
  0x40,
};
const struct IrCode code_114Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_114Times,
  code_114Codes
};


const uint16_t code_115Times[] PROGMEM = {
  81, 86,
  81, 296,
  81, 3349,
  328, 331,
  329, 331,
};
const uint8_t code_115Codes[] PROGMEM = {
  0x60,
  0x82,
  0x00,
  0x20,
  0x80,
  0x41,
  0x04,
  0x90,
  0x41,
  0x2A,
  0x02,
  0x08,
  0x00,
  0x82,
  0x01,
  0x04,
  0x12,
  0x41,
  0x04,
  0x80,
};
const struct IrCode code_115Code PROGMEM = {
  freq_to_timerval(40000),
  52,		// # of pairs
  3,		// # of bits per index
  code_115Times,
  code_115Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_116Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_116Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x40,
  0x04,
  0x92,
  0x09,
  0x24,
  0x00,
  0x40,
  0x00,
  0x92,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_116Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_116Codes
};
const uint16_t code_117Times[] PROGMEM = {
  49, 54,
  49, 158,
  49, 420,
  49, 2446,
  819, 420,
  821, 419,
};
const uint8_t code_117Codes[] PROGMEM = {
  0x84,
  0x00,
  0x00,
  0x08,
  0x12,
  0x40,
  0x01,
  0xD2,
  0x00,
  0x00,
  0x04,
  0x09,
  0x20,
  0x00,
  0x40,
};
const struct IrCode code_117Code PROGMEM = {
  freq_to_timerval(41667),
  38,		// # of pairs
  3,		// # of bits per index
  code_117Times,
  code_117Codes
};

/* Duplicate timing table, same as na044 !
 const uint16_t code_118Times[] PROGMEM = {
 	51, 51,
 	51, 160,
 	51, 4096,
 	51, 9513,
 	431, 436,
 	883, 219,
 };
 */
const uint8_t code_118Codes[] PROGMEM = {
  0x84,
  0x90,
  0x49,
  0x20,
  0x02,
  0x00,
  0x04,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0xBA,
  0x40,
};
const struct IrCode code_118Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_044Times,
  code_118Codes
};
const uint16_t code_119Times[] PROGMEM = {
  55, 63,
  55, 171,
  55, 4094,
  55, 9508,
  881, 219,
  881, 438,
};
const uint8_t code_119Codes[] PROGMEM = {
  0xA0,
  0x10,
  0x00,
  0x04,
  0x82,
  0x49,
  0x20,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_119Code PROGMEM = {
  freq_to_timerval(55556),
  38,		// # of pairs
  3,		// # of bits per index
  code_119Times,
  code_119Codes
};


/* Duplicate timing table, same as na017 !
 const uint16_t code_120Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_120Codes[] PROGMEM = {
  0xA0,
  0x12,
  0x00,
  0x04,
  0x80,
  0x49,
  0x24,
  0x92,
  0x40,
  0x00,
  0x00,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_120Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_120Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_121Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_121Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x40,
  0x04,
  0x92,
  0x09,
  0x20,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_121Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_121Codes
};
const uint16_t code_122Times[] PROGMEM = {
  80, 95,
  80, 249,
  80, 3867,
  81, 0,
  329, 322,
};
const uint8_t code_122Codes[] PROGMEM = {
  0x80,
  0x00,
  0x00,
  0x00,
  0x12,
  0x49,
  0x24,
  0x90,
  0x0A,
  0x80,
  0x00,
  0x00,
  0x00,
  0x12,
  0x49,
  0x24,
  0x90,
  0x0B,
};
const struct IrCode code_122Code PROGMEM = {
  freq_to_timerval(52632),
  48,		// # of pairs
  3,		// # of bits per index
  code_122Times,
  code_122Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_123Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_123Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x48,
  0x04,
  0x90,
  0x01,
  0x20,
  0x12,
  0x40,
  0x04,
  0x80,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_123Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_123Codes
};
const uint16_t code_124Times[] PROGMEM = {
  54, 56,
  54, 151,
  54, 4092,
  54, 8677,
  900, 421,
  901, 226,
};
const uint8_t code_124Codes[] PROGMEM = {
  0x80,
  0x00,
  0x48,
  0x04,
  0x92,
  0x01,
  0x20,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_124Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_124Times,
  code_124Codes
};

/* Duplicate timing table, same as na119 !
 const uint16_t code_125Times[] PROGMEM = {
 	55, 63,
 	55, 171,
 	55, 4094,
 	55, 9508,
 	881, 219,
 	881, 438,
 };
 */
const uint8_t code_125Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x48,
  0x04,
  0x90,
  0x01,
  0x20,
  0x80,
  0x40,
  0x04,
  0x12,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_125Code PROGMEM = {
  freq_to_timerval(55556),
  38,		// # of pairs
  3,		// # of bits per index
  code_119Times,
  code_125Codes
};


/* Duplicate timing table, same as na017 !
 const uint16_t code_126Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_126Codes[] PROGMEM = {
  0xA4,
  0x10,
  0x00,
  0x20,
  0x82,
  0x49,
  0x00,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_126Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_126Codes
};
const uint16_t code_127Times[] PROGMEM = {
  114, 100,
  115, 100,
  115, 200,
  115, 2706,
};
const uint8_t code_127Codes[] PROGMEM = {
  0x1B,
  0x59,
};
const struct IrCode code_127Code PROGMEM = {
  freq_to_timerval(25641),
  8,		// # of pairs
  2,		// # of bits per index
  code_127Times,
  code_127Codes
};

/* Duplicate timing table, same as na102 !
 const uint16_t code_128Times[] PROGMEM = {
 	86, 87,
 	86, 258,
 	86, 3338,
 	346, 348,
 	348, 347,
 };
 */
const uint8_t code_128Codes[] PROGMEM = {
  0x60,
  0x02,
  0x08,
  0x00,
  0x02,
  0x49,
  0x04,
  0x12,
  0x49,
  0x0A,
  0x00,
  0x08,
  0x20,
  0x00,
  0x09,
  0x24,
  0x10,
  0x49,
  0x24,
  0x00,
};
const struct IrCode code_128Code PROGMEM = {
  freq_to_timerval(40000),
  52,		// # of pairs
  3,		// # of bits per index
  code_102Times,
  code_128Codes
};

/* Duplicate timing table, same as na017 !
 const uint16_t code_129Times[] PROGMEM = {
 	56, 57,
 	56, 175,
 	56, 4150,
 	56, 9499,
 	898, 227,
 	898, 449,
 };
 */
const uint8_t code_129Codes[] PROGMEM = {
  0xA4,
  0x92,
  0x49,
  0x20,
  0x00,
  0x00,
  0x00,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x38,
  0x40,
};
const struct IrCode code_129Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_017Times,
  code_129Codes
};
const uint16_t code_130Times[] PROGMEM = {
  88, 90,
  88, 258,
  88, 2247,
  358, 349,
  358, 350,
};
const uint8_t code_130Codes[] PROGMEM = {
  0x64,
  0x00,
  0x08,
  0x24,
  0x82,
  0x09,
  0x24,
  0x10,
  0x01,
  0x0A,
  0x10,
  0x00,
  0x20,
  0x92,
  0x08,
  0x24,
  0x90,
  0x40,
  0x04,
  0x10,
};
const struct IrCode code_130Code PROGMEM = {
  freq_to_timerval(37037),
  52,		// # of pairs
  3,		// # of bits per index
  code_130Times,
  code_130Codes
};

/* Duplicate timing table, same as na042 !
 const uint16_t code_131Times[] PROGMEM = {
 	54, 65,
 	54, 170,
 	54, 4099,
 	54, 8668,
 	899, 226,
 	899, 421,
 };
 */
const uint8_t code_131Codes[] PROGMEM = {
  0xA0,
  0x10,
  0x40,
  0x04,
  0x82,
  0x09,
  0x24,
  0x82,
  0x40,
  0x00,
  0x10,
  0x09,
  0x2A,
  0x38,
  0x00,
};
const struct IrCode code_131Code PROGMEM = {
  freq_to_timerval(40000),
  38,		// # of pairs
  3,		// # of bits per index
  code_042Times,
  code_131Codes
};
const uint16_t code_132Times[] PROGMEM = {
  28, 106,
  28, 238,
  28, 370,
  28, 1173,
};
const uint8_t code_132Codes[] PROGMEM = {
  0x22,
  0x20,
  0x00,
  0x17,
  0x22,
  0x20,
  0x00,
  0x14,
};
const struct IrCode code_132Code PROGMEM = {
  freq_to_timerval(83333),
  32,		// # of pairs
  2,		// # of bits per index
  code_132Times,
  code_132Codes
};
const uint16_t code_133Times[] PROGMEM = {
  13, 741,
  15, 489,
  15, 740,
  17, 4641,
  18, 0,
};
const uint8_t code_133Codes[] PROGMEM = {
  0x09,
  0x24,
  0x49,
  0x48,
  0xB4,
  0x92,
  0x44,
  0x94,
  0x8C,
};
const struct IrCode code_133Code PROGMEM = {
  freq_to_timerval(41667),
  24,		// # of pairs
  3,		// # of bits per index
  code_133Times,
  code_133Codes
};

/* Duplicate timing table, same as na113 !
 const uint16_t code_134Times[] PROGMEM = {
 	56, 54,
 	56, 166,
 	56, 3945,
 	896, 442,
 	896, 443,
 };
 */
const uint8_t code_134Codes[] PROGMEM = {
  0x60,
  0x90,
  0x00,
  0x24,
  0x10,
  0x00,
  0x04,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0x02,
  0x40,
  0x00,
  0x90,
  0x40,
  0x00,
  0x12,
  0x48,
  0x00,
  0x00,
  0x01,
  0x24,
  0x80,
};
const struct IrCode code_134Code PROGMEM = {
  freq_to_timerval(40000),
  68,		// # of pairs
  3,		// # of bits per index
  code_113Times,
  code_134Codes
};
const uint16_t code_135Times[] PROGMEM = {
  53, 59,
  53, 171,
  53, 2301,
  892, 450,
  895, 448,
};
const uint8_t code_135Codes[] PROGMEM = {
  0x60,
  0x12,
  0x49,
  0x00,
  0x00,
  0x09,
  0x00,
  0x00,
  0x49,
  0x24,
  0x80,
  0x00,
  0x00,
  0x12,
  0x49,
  0x24,
  0xA8,
  0x01,
  0x24,
  0x90,
  0x00,
  0x00,
  0x90,
  0x00,
  0x04,
  0x92,
  0x48,
  0x00,
  0x00,
  0x01,
  0x24,
  0x92,
  0x48,
};
const struct IrCode code_135Code PROGMEM = {
  freq_to_timerval(38462),
  88,		// # of pairs
  3,		// # of bits per index
  code_135Times,
  code_135Codes
};
const uint16_t code_136Times[] PROGMEM = {
  53, 59,
  53, 171,
  53, 2301,
  55, 0,
  892, 450,
  895, 448,
};
const uint8_t code_136Codes[] PROGMEM = {
  0x84,
  0x82,
  0x49,
  0x00,
  0x00,
  0x00,
  0x20,
  0x00,
  0x49,
  0x24,
  0x80,
  0x00,
  0x00,
  0x12,
  0x49,
  0x24,
  0xAA,
  0x48,
  0x24,
  0x90,
  0x00,
  0x00,
  0x02,
  0x00,
  0x04,
  0x92,
  0x48,
  0x00,
  0x00,
  0x01,
  0x24,
  0x92,
  0x4B,
};
const struct IrCode code_136Code PROGMEM = {
  freq_to_timerval(38610),
  88,		// # of pairs
  3,		// # of bits per index
  code_136Times,
  code_136Codes
};

const struct IrCode *NApowerCodes[] PROGMEM = {
	&code_000Code,
	&code_001Code,
	&code_002Code,
	&code_003Code,
	&code_004Code,
	&code_005Code,
	&code_006Code,
	&code_007Code,
	&code_008Code,
	&code_009Code,
	&code_010Code,
	&code_011Code,
	&code_012Code,
	&code_013Code,
	&code_014Code,
	&code_015Code,
	&code_016Code,
	&code_017Code,
	&code_018Code,
	&code_019Code,
	&code_020Code,
	&code_021Code,
	&code_022Code,
	&code_023Code,
	&code_024Code,
	&code_025Code,
	&code_026Code,
	&code_027Code,
	&code_028Code,
	&code_029Code,
	&code_030Code,
	&code_031Code,
	&code_032Code,
	&code_033Code,
	&code_034Code,
	&code_035Code,
	&code_036Code,
	&code_037Code,
	&code_038Code,
	&code_039Code,
	&code_040Code,
	&code_041Code,
	&code_042Code,
	&code_043Code,
	&code_044Code,
	&code_045Code,
	&code_046Code,
	&code_047Code,
	&code_048Code,
	&code_049Code,
	&code_050Code,
	&code_051Code,
	&code_052Code,
	&code_053Code,
	&code_054Code,
	&code_055Code,
	&code_056Code,
	&code_057Code,
	&code_058Code,
	&code_059Code,
	&code_060Code,
	&code_061Code,
	&code_062Code,
	&code_063Code,
	&code_064Code,
	&code_065Code,
	&code_066Code,
	&code_067Code,
	&code_068Code,
	&code_069Code,
	&code_070Code,
	&code_071Code,
	&code_072Code,
	&code_073Code,
	&code_074Code,
	&code_075Code,
	&code_076Code,
	&code_077Code,
	&code_078Code,
	&code_079Code,
	&code_080Code,
	&code_081Code,
	&code_082Code,
	&code_083Code,
	&code_084Code,
	&code_085Code,
	&code_086Code,
	&code_087Code,
	&code_088Code,
	&code_089Code,
	&code_090Code,
	&code_091Code,
	&code_092Code,
	&code_093Code,
	&code_094Code,
	&code_095Code,
	&code_096Code,
	&code_097Code,
	&code_098Code,
	&code_099Code,
	&code_100Code,
	&code_101Code,
	&code_102Code,
	&code_103Code,
	&code_104Code,
	&code_105Code,
	&code_106Code,
	&code_107Code,
	&code_108Code,
	&code_109Code,
	&code_110Code,
	&code_111Code,
	&code_112Code,
	&code_113Code,
	&code_114Code,
	&code_115Code,
	&code_116Code,
	&code_117Code,
	&code_118Code,
	&code_119Code,
	&code_120Code,
	&code_121Code,
	&code_122Code,
	&code_123Code,
	&code_124Code,
	&code_125Code,
	&code_126Code,
	&code_127Code,
	&code_128Code,
	&code_129Code,
	&code_130Code,
	&code_131Code,
	&code_132Code,
	&code_133Code,
	&code_134Code,
	&code_135Code,
	&code_136Code,
};
uint8_t num_NAcodes = NUM_ELEM(NApowerCodes);

#endif

#ifdef UseEUcodes


const uint16_t code_000Times[] PROGMEM = {
  43, 47,
  43, 91,
  43, 8324,
  88, 47,
  133, 133,
  264, 90,
  264, 91,
};
const uint8_t code_000Codes[] PROGMEM = {
  0xA4,
  0x08,
  0x00,
  0x00,
  0x00,
  0x00,
  0x64,
  0x2C,
  0x40,
  0x80,
  0x00,
  0x00,
  0x00,
  0x06,
  0x41,
};
const struct IrCode code_000Code PROGMEM = {
  freq_to_timerval(35714),
  40,		// # of pairs
  3,		// # of bits per index
  code_000Times,
  code_000Codes
};
const uint16_t code_001Times[] PROGMEM = {
  47, 265,
  51, 54,
  51, 108,
  51, 263,
  51, 2053,
  51, 11647,
  100, 109,
};
const uint8_t code_001Codes[] PROGMEM = {
  0x04,
  0x92,
  0x49,
  0x26,
  0x35,
  0x89,
  0x24,
  0x9A,
  0xD6,
  0x24,
  0x92,
  0x48,
};
const struct IrCode code_001Code PROGMEM = {
  freq_to_timerval(30303),
  31,		// # of pairs
  3,		// # of bits per index
  code_001Times,
  code_001Codes
};
const uint16_t code_002Times[] PROGMEM = {
  43, 206,
  46, 204,
  46, 456,
  46, 3488,
};
const uint8_t code_002Codes[] PROGMEM = {
  0x1A,
  0x56,
  0xA6,
  0xD6,
  0x95,
  0xA9,
  0x90,
};
const struct IrCode code_002Code PROGMEM = {
  freq_to_timerval(33333),
  26,		// # of pairs
  2,		// # of bits per index
  code_002Times,
  code_002Codes
};

const uint16_t code_004Times[] PROGMEM = {
  44, 45,
  44, 131,
  44, 7462,
  346, 176,
  346, 178,
};
const uint8_t code_004Codes[] PROGMEM = {
  0x60,
  0x80,
  0x00,
  0x00,
  0x00,
  0x08,
  0x00,
  0x00,
  0x00,
  0x20,
  0x00,
  0x00,
  0x04,
  0x12,
  0x48,
  0x04,
  0x12,
  0x48,
  0x2A,
  0x02,
  0x00,
  0x00,
  0x00,
  0x00,
  0x20,
  0x00,
  0x00,
  0x00,
  0x80,
  0x00,
  0x00,
  0x10,
  0x49,
  0x20,
  0x10,
  0x49,
  0x20,
  0x80,
};
const struct IrCode code_004Code PROGMEM = {
  freq_to_timerval(37037),
  100,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_004Codes
};// Duplicate IR Code? Similar to NA002

const uint16_t code_005Times[] PROGMEM = {
  24, 190,
  25, 80,
  25, 190,
  25, 4199,
  25, 4799,
};
const uint8_t code_005Codes[] PROGMEM = {
  0x04,
  0x92,
  0x52,
  0x28,
  0x92,
  0x8C,
  0x44,
  0x92,
  0x89,
  0x45,
  0x24,
  0x53,
  0x44,
  0x92,
  0x52,
  0x28,
  0x92,
  0x8C,
  0x44,
  0x92,
  0x89,
  0x45,
  0x24,
  0x51,
};
const struct IrCode code_005Code PROGMEM = {
  freq_to_timerval(38610),
  64,		// # of pairs
  3,		// # of bits per index
  code_005Times,
  code_005Codes
};
const uint16_t code_006Times[] PROGMEM = {
  53, 63,
  53, 172,
  53, 4472,
  54, 0,
  455, 468,
};
const uint8_t code_006Codes[] PROGMEM = {
  0x84,
  0x90,
  0x00,
  0x04,
  0x90,
  0x00,
  0x00,
  0x80,
  0x00,
  0x04,
  0x12,
  0x49,
  0x2A,
  0x12,
  0x40,
  0x00,
  0x12,
  0x40,
  0x00,
  0x02,
  0x00,
  0x00,
  0x10,
  0x49,
  0x24,
  0xB0,
};
const struct IrCode code_006Code PROGMEM = {
  freq_to_timerval(38462),
  68,		// # of pairs
  3,		// # of bits per index
  code_006Times,
  code_006Codes
};
const uint16_t code_007Times[] PROGMEM = {
  50, 54,
  50, 159,
  50, 2307,
  838, 422,
};
const uint8_t code_007Codes[] PROGMEM = {
  0xD4,
  0x00,
  0x15,
  0x10,
  0x25,
  0x00,
  0x05,
  0x44,
  0x09,
  0x40,
  0x01,
  0x51,
  0x01,
};
const struct IrCode code_007Code PROGMEM = {
  freq_to_timerval(38462),
  52,		// # of pairs
  2,		// # of bits per index
  code_007Times,
  code_007Codes
};// Duplicate IR Code? - Similar to NA010


/* Duplicate timing table, same as na004 !
 const uint16_t code_008Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_008Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x41,
  0x04,
  0x92,
  0x08,
  0x24,
  0x90,
  0x40,
  0x00,
  0x02,
  0x09,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_008Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_008Codes
};


/* Duplicate timing table, same as na005 !
 const uint16_t code_009Times[] PROGMEM = {
 	88, 90,
 	88, 91,
 	88, 181,
 	88, 8976,
 	177, 91,
 };
 */
/*
const uint8_t code_009Codes[] PROGMEM = {
 	0x10,
 	0x92,
 	0x49,
 	0x46,
 	0x33,
 	0x09,
 	0x24,
 	0x94,
 	0x60,
 };
 const struct IrCode code_009Code PROGMEM = {
 	freq_to_timerval(35714),
 	24,		// # of pairs
 	3,		// # of bits per index
 	code_005Times,
 	code_009Codes
 };// Duplicate IR Code - same as na005
 */


/* Duplicate timing table, same as na004 !
 const uint16_t code_010Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
/*
const uint8_t code_010Codes[] PROGMEM = {
 	0xA0,
 	0x00,
 	0x01,
 	0x04,
 	0x92,
 	0x48,
 	0x20,
 	0x80,
 	0x40,
 	0x04,
 	0x12,
 	0x09,
 	0x2B,
 	0x3D,
 	0x00,
 };
 const struct IrCode code_010Code PROGMEM = {
 	freq_to_timerval(38462),
 	38,		// # of pairs
 	3,		// # of bits per index
 	code_004Times,
 	code_010Codes
 };// Duplicate IR Code - same as NA004
 */

/* Duplicate timing table, same as na009 !
 const uint16_t code_011Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_011Codes[] PROGMEM = {
  0x84,
  0x00,
  0x48,
  0x04,
  0x02,
  0x01,
  0x04,
  0x80,
  0x09,
  0x00,
  0x12,
  0x40,
  0x2A,
  0xBA,
  0x40,
};
const struct IrCode code_011Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_011Codes
};
const uint16_t code_012Times[] PROGMEM = {
  46, 206,
  46, 459,
  46, 3447,
};
const uint8_t code_012Codes[] PROGMEM = {
  0x05,
  0x01,
  0x51,
  0x81,
  0x40,
  0x54,
  0x40,
};
const struct IrCode code_012Code PROGMEM = {
  freq_to_timerval(33445),
  26,		// # of pairs
  2,		// # of bits per index
  code_012Times,
  code_012Codes
};
const uint16_t code_013Times[] PROGMEM = {
  53, 59,
  53, 171,
  53, 2302,
  895, 449,
};
const uint8_t code_013Codes[] PROGMEM = {
  0xD4,
  0x55,
  0x00,
  0x00,
  0x40,
  0x15,
  0x54,
  0x00,
  0x01,
  0x55,
  0x56,
  0xD4,
  0x55,
  0x00,
  0x00,
  0x40,
  0x15,
  0x54,
  0x00,
  0x01,
  0x55,
  0x55,
};
const struct IrCode code_013Code PROGMEM = {
  freq_to_timerval(38462),
  88,		// # of pairs
  2,		// # of bits per index
  code_013Times,
  code_013Codes
};

/* Duplicate timing table, same as na021 !
 const uint16_t code_014Times[] PROGMEM = {
 	48, 52,
 	48, 160,
 	48, 400,
 	48, 2335,
 	799, 400,
 };
 */
/*
const uint8_t code_014Codes[] PROGMEM = {
 	0x80,
 	0x10,
 	0x40,
 	0x08,
 	0x82,
 	0x08,
 	0x01,
 	0xC0,
 	0x08,
 	0x20,
 	0x04,
 	0x41,
 	0x04,
 	0x00,
 	0x00,
 };
 const struct IrCode code_014Code PROGMEM = {
 	freq_to_timerval(38462),
 	38,		// # of pairs
 	3,		// # of bits per index
 	code_021Times,
 	code_014Codes
 };// Duplicate IR Code - same as NA021
 */

const uint16_t code_015Times[] PROGMEM = {
  53, 54,
  53, 156,
  53, 2542,
  851, 425,
  853, 424,
};
const uint8_t code_015Codes[] PROGMEM = {
  0x60,
  0x82,
  0x08,
  0x24,
  0x10,
  0x41,
  0x00,
  0x12,
  0x40,
  0x04,
  0x80,
  0x09,
  0x2A,
  0x02,
  0x08,
  0x20,
  0x90,
  0x41,
  0x04,
  0x00,
  0x49,
  0x00,
  0x12,
  0x00,
  0x24,
  0xA8,
  0x08,
  0x20,
  0x82,
  0x41,
  0x04,
  0x10,
  0x01,
  0x24,
  0x00,
  0x48,
  0x00,
  0x92,
  0xA0,
  0x20,
  0x82,
  0x09,
  0x04,
  0x10,
  0x40,
  0x04,
  0x90,
  0x01,
  0x20,
  0x02,
  0x48,
};
const struct IrCode code_015Code PROGMEM = {
  freq_to_timerval(38462),
  136,		// # of pairs
  3,		// # of bits per index
  code_015Times,
  code_015Codes
};// Duplicate IR Code? - Similar to NA018

const uint16_t code_016Times[] PROGMEM = {
  28, 92,
  28, 213,
  28, 214,
  28, 2771,
};
const uint8_t code_016Codes[] PROGMEM = {
  0x68,
  0x08,
  0x20,
  0x00,
  0xEA,
  0x02,
  0x08,
  0x00,
  0x10,
};
const struct IrCode code_016Code PROGMEM = {
  freq_to_timerval(33333),
  34,		// # of pairs
  2,		// # of bits per index
  code_016Times,
  code_016Codes
};
const uint16_t code_017Times[] PROGMEM = {
  15, 844,
  16, 557,
  16, 844,
  16, 5224,
};
const uint8_t code_017Codes[] PROGMEM = {
  0x1A,
  0x9A,
  0x9B,
  0x9A,
  0x9A,
  0x99,
};
const struct IrCode code_017Code PROGMEM = {
  freq_to_timerval(33333),
  24,		// # of pairs
  2,		// # of bits per index
  code_017Times,
  code_017Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_018Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_018Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x48,
  0x04,
  0x90,
  0x01,
  0x20,
  0x12,
  0x40,
  0x04,
  0x80,
  0x09,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_018Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_018Codes
};
const uint16_t code_019Times[] PROGMEM = {
  50, 54,
  50, 158,
  50, 418,
  50, 2443,
  843, 418,
};
const uint8_t code_019Codes[] PROGMEM = {
  0x80,
  0x80,
  0x00,
  0x08,
  0x12,
  0x40,
  0x01,
  0xC0,
  0x40,
  0x00,
  0x04,
  0x09,
  0x20,
  0x00,
  0x00,
};
const struct IrCode code_019Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_019Times,
  code_019Codes
};
const uint16_t code_020Times[] PROGMEM = {
  48, 301,
  48, 651,
  48, 1001,
  48, 3001,
};
const uint8_t code_020Codes[] PROGMEM = {
  0x22,
  0x20,
  0x00,
  0x01,
  0xC8,
  0x88,
  0x00,
  0x00,
  0x40,
};
const struct IrCode code_020Code PROGMEM = {
  freq_to_timerval(35714),
  34,		// # of pairs
  2,		// # of bits per index
  code_020Times,
  code_020Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_021Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_021Codes[] PROGMEM = {
  0x84,
  0x80,
  0x00,
  0x20,
  0x82,
  0x49,
  0x00,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0xBA,
  0x40,
};
const struct IrCode code_021Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_021Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_022Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_022Codes[] PROGMEM = {
  0xA4,
  0x80,
  0x41,
  0x00,
  0x12,
  0x08,
  0x24,
  0x90,
  0x40,
  0x00,
  0x02,
  0x09,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_022Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_022Codes
};

/* Duplicate timing table, same as na022 !
 const uint16_t code_023Times[] PROGMEM = {
 	53, 60,
 	53, 175,
 	53, 4463,
 	53, 9453,
 	892, 450,
 	895, 225,
 };
 */
/*
const uint8_t code_023Codes[] PROGMEM = {
 	0x80,
 	0x02,
 	0x40,
 	0x00,
 	0x02,
 	0x40,
 	0x00,
 	0x00,
 	0x01,
 	0x24,
 	0x92,
 	0x48,
 	0x0A,
 	0xBA,
 	0x00,
 };
 const struct IrCode code_023Code PROGMEM = {
 	freq_to_timerval(38462),
 	38,		// # of pairs
 	3,		// # of bits per index
 	code_022Times,
 	code_023Codes
 };// Duplicate IR Code - Same as NA022
 */


/* Duplicate timing table, same as na004 !
 const uint16_t code_024Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_024Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x48,
  0x04,
  0x90,
  0x01,
  0x20,
  0x00,
  0x40,
  0x04,
  0x92,
  0x09,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_024Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_024Codes
};
const uint16_t code_025Times[] PROGMEM = {
  49, 52,
  49, 102,
  49, 250,
  49, 252,
  49, 2377,
  49, 12009,
  100, 52,
  100, 102,
};
const uint8_t code_025Codes[] PROGMEM = {
  0x47,
  0x00,
  0x23,
  0x3C,
  0x01,
  0x59,
  0xE0,
  0x04,
};
const struct IrCode code_025Code PROGMEM = {
  freq_to_timerval(31250),
  21,		// # of pairs
  3,		// # of bits per index
  code_025Times,
  code_025Codes
};
const uint16_t code_026Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 4926,
};
const uint8_t code_026Codes[] PROGMEM = {
  0x55,
  0x40,
  0x42,
  0x55,
  0x40,
  0x41,
};
const struct IrCode code_026Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_026Times,
  code_026Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_027Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_027Codes[] PROGMEM = {
  0xA0,
  0x82,
  0x08,
  0x24,
  0x10,
  0x41,
  0x04,
  0x10,
  0x01,
  0x20,
  0x82,
  0x48,
  0x0B,
  0x3D,
  0x00,
};
const struct IrCode code_027Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_027Codes
};
const uint16_t code_028Times[] PROGMEM = {
  47, 267,
  50, 55,
  50, 110,
  50, 265,
  50, 2055,
  50, 12117,
  100, 57,
};
const uint8_t code_028Codes[] PROGMEM = {
  0x04,
  0x92,
  0x49,
  0x26,
  0x34,
  0x72,
  0x24,
  0x9A,
  0xD1,
  0xC8,
  0x92,
  0x48,
};
const struct IrCode code_028Code PROGMEM = {
  freq_to_timerval(30303),
  31,		// # of pairs
  3,		// # of bits per index
  code_028Times,
  code_028Codes
};
const uint16_t code_029Times[] PROGMEM = {
  50, 50,
  50, 99,
  50, 251,
  50, 252,
  50, 1445,
  50, 11014,
  102, 49,
  102, 98,
};
const uint8_t code_029Codes[] PROGMEM = {
  0x47,
  0x00,
  0x00,
  0x00,
  0x00,
  0x04,
  0x64,
  0x62,
  0x00,
  0xE0,
  0x00,
  0x2B,
  0x23,
  0x10,
  0x07,
  0x00,
  0x00,
  0x80,
};
const struct IrCode code_029Code PROGMEM = {
  freq_to_timerval(34483),
  46,		// # of pairs
  3,		// # of bits per index
  code_029Times,
  code_029Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_030Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_030Codes[] PROGMEM = {
  0xA0,
  0x10,
  0x00,
  0x04,
  0x82,
  0x49,
  0x20,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_030Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_030Codes
};// Duplicate IR Code? - Smilar to NA020

const uint16_t code_031Times[] PROGMEM = {
  53, 53,
  53, 160,
  53, 1697,
  838, 422,
};
const uint8_t code_031Codes[] PROGMEM = {
  0xD5,
  0x50,
  0x15,
  0x11,
  0x65,
  0x54,
  0x05,
  0x44,
  0x59,
  0x55,
  0x01,
  0x51,
  0x15,
};
const struct IrCode code_031Code PROGMEM = {
  freq_to_timerval(38462),
  52,		// # of pairs
  2,		// # of bits per index
  code_031Times,
  code_031Codes
};
const uint16_t code_032Times[] PROGMEM = {
  49, 205,
  49, 206,
  49, 456,
  49, 3690,
};
const uint8_t code_032Codes[] PROGMEM = {
  0x1A,
  0x56,
  0xA5,
  0xD6,
  0x95,
  0xA9,
  0x40,
};
const struct IrCode code_032Code PROGMEM = {
  freq_to_timerval(33333),
  26,		// # of pairs
  2,		// # of bits per index
  code_032Times,
  code_032Codes
};
const uint16_t code_033Times[] PROGMEM = {
  48, 150,
  50, 149,
  50, 347,
  50, 2936,
};
const uint8_t code_033Codes[] PROGMEM = {
  0x2A,
  0x5D,
  0xA9,
  0x60,
};
const struct IrCode code_033Code PROGMEM = {
  freq_to_timerval(38462),
  14,		// # of pairs
  2,		// # of bits per index
  code_033Times,
  code_033Codes
};


/* Duplicate timing table, same as na004 !
 const uint16_t code_034Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_034Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x40,
  0x04,
  0x90,
  0x09,
  0x20,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_034Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_034Codes
};

/* Duplicate timing table, same as na005 !
 const uint16_t code_035Times[] PROGMEM = {
 	88, 90,
 	88, 91,
 	88, 181,
 	88, 8976,
 	177, 91,
 };
 */
/*
const uint8_t code_035Codes[] PROGMEM = {
 	0x10,
 	0x92,
 	0x49,
 	0x46,
 	0x33,
 	0x09,
 	0x24,
 	0x94,
 	0x60,
 };
 const struct IrCode code_035Code PROGMEM = {
 	freq_to_timerval(35714),
 	24,		// # of pairs
 	3,		// # of bits per index
 	code_005Times,
 	code_035Codes
 };// Duplicate IR Code - same as eu009!
 */

/* Duplicate timing table, same as na004 !
 const uint16_t code_036Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_036Codes[] PROGMEM = {
  0xA4,
  0x00,
  0x49,
  0x00,
  0x92,
  0x00,
  0x20,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_036Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_036Codes
};
const uint16_t code_037Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 5178,
};
const uint8_t code_037Codes[] PROGMEM = {
  0x45,
  0x50,
  0x02,
  0x45,
  0x50,
  0x01,
};
const struct IrCode code_037Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_037Times,
  code_037Codes
};
const uint16_t code_038Times[] PROGMEM = {
  3, 1002,
  3, 1495,
  3, 3059,
};
const uint8_t code_038Codes[] PROGMEM = {
  0x05,
  0x60,
  0x54,
};
const struct IrCode code_038Code PROGMEM = {
  0,              // Non-pulsed code
  11,		// # of pairs
  2,		// # of bits per index
  code_038Times,
  code_038Codes
};
const uint16_t code_039Times[] PROGMEM = {
  13, 445,
  13, 674,
  13, 675,
  13, 4583,
};
const uint8_t code_039Codes[] PROGMEM = {
  0x6A,
  0x82,
  0x83,
  0xAA,
  0x82,
  0x81,
};
const struct IrCode code_039Code PROGMEM = {
  freq_to_timerval(40161),
  24,		// # of pairs
  2,		// # of bits per index
  code_039Times,
  code_039Codes
};
const uint16_t code_040Times[] PROGMEM = {
  85, 89,
  85, 264,
  85, 3402,
  347, 350,
  348, 350,
};
const uint8_t code_040Codes[] PROGMEM = {
  0x60,
  0x90,
  0x40,
  0x20,
  0x80,
  0x40,
  0x20,
  0x90,
  0x41,
  0x2A,
  0x02,
  0x41,
  0x00,
  0x82,
  0x01,
  0x00,
  0x82,
  0x41,
  0x04,
  0x80,
};
const struct IrCode code_040Code PROGMEM = {
  freq_to_timerval(35714),
  52,		// # of pairs
  3,		// # of bits per index
  code_040Times,
  code_040Codes
};
const uint16_t code_041Times[] PROGMEM = {
  46, 300,
  49, 298,
  49, 648,
  49, 997,
  49, 3056,
};
const uint8_t code_041Codes[] PROGMEM = {
  0x0C,
  0xB2,
  0xCA,
  0x49,
  0x13,
  0x0B,
  0x2C,
  0xB2,
  0x92,
  0x44,
  0xB0,
};
const struct IrCode code_041Code PROGMEM = {
  freq_to_timerval(33333),
  28,		// # of pairs
  3,		// # of bits per index
  code_041Times,
  code_041Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_042Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_042Codes[] PROGMEM = {
  0x80,
  0x00,
  0x00,
  0x24,
  0x92,
  0x09,
  0x00,
  0x82,
  0x00,
  0x04,
  0x10,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_042Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_042Codes
};
const uint16_t code_043Times[] PROGMEM = {
  1037, 4216,
  1040, 0,
};
const uint8_t code_043Codes[] PROGMEM = {
  0x10,
};
const struct IrCode code_043Code PROGMEM = {
  freq_to_timerval(41667),
  2,		// # of pairs
  2,		// # of bits per index
  code_043Times,
  code_043Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_044Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_044Codes[] PROGMEM = {
  0xA0,
  0x02,
  0x01,
  0x04,
  0x90,
  0x48,
  0x20,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_044Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_044Codes
};
const uint16_t code_045Times[] PROGMEM = {
  152, 471,
  154, 156,
  154, 469,
  154, 2947,
};
const uint8_t code_045Codes[] PROGMEM = {
  0x16,
  0xE5,
  0x90,
};
const struct IrCode code_045Code PROGMEM = {
  freq_to_timerval(41667),
  10,		// # of pairs
  2,		// # of bits per index
  code_045Times,
  code_045Codes
};
const uint16_t code_046Times[] PROGMEM = {
  15, 493,
  16, 493,
  16, 698,
  16, 1414,
};
const uint8_t code_046Codes[] PROGMEM = {
  0x16,
  0xAB,
  0x56,
  0xA9,
};
const struct IrCode code_046Code PROGMEM = {
  freq_to_timerval(34602),
  16,		// # of pairs
  2,		// # of bits per index
  code_046Times,
  code_046Codes
};
const uint16_t code_047Times[] PROGMEM = {
  3, 496,
  3, 745,
  3, 1488,
};
const uint8_t code_047Codes[] PROGMEM = {
  0x41,
  0x24,
  0x12,
  0x41,
  0x00,
};
const struct IrCode code_047Code PROGMEM = {
  0,              // Non-pulsed code
  17,		// # of pairs
  2,		// # of bits per index
  code_047Times,
  code_047Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_048Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_048Codes[] PROGMEM = {
  0x80,
  0x00,
  0x00,
  0x24,
  0x82,
  0x49,
  0x04,
  0x80,
  0x40,
  0x00,
  0x12,
  0x09,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_048Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_048Codes
};
const uint16_t code_049Times[] PROGMEM = {
  55, 55,
  55, 167,
  55, 4577,
  55, 9506,
  448, 445,
  450, 444,
};
const uint8_t code_049Codes[] PROGMEM = {
  0x80,
  0x92,
  0x00,
  0x00,
  0x92,
  0x00,
  0x00,
  0x10,
  0x40,
  0x04,
  0x82,
  0x09,
  0x2A,
  0x97,
  0x48,
};
const struct IrCode code_049Code PROGMEM = {
  freq_to_timerval(38462),
  40,		// # of pairs
  3,		// # of bits per index
  code_049Times,
  code_049Codes
};
const uint16_t code_050Times[] PROGMEM = {
  91, 88,
  91, 267,
  91, 3621,
  361, 358,
  361, 359,
};
const uint8_t code_050Codes[] PROGMEM = {
  0x60,
  0x00,
  0x00,
  0x00,
  0x12,
  0x49,
  0x24,
  0x92,
  0x42,
  0x80,
  0x00,
  0x00,
  0x00,
  0x12,
  0x49,
  0x24,
  0x92,
  0x40,
};
const struct IrCode code_050Code PROGMEM = {
  freq_to_timerval(33333),
  48,		// # of pairs
  3,		// # of bits per index
  code_050Times,
  code_050Codes
};
const uint16_t code_051Times[] PROGMEM = {
  84, 88,
  84, 261,
  84, 3360,
  347, 347,
  347, 348,
};
const uint8_t code_051Codes[] PROGMEM = {
  0x60,
  0x82,
  0x00,
  0x20,
  0x80,
  0x41,
  0x04,
  0x90,
  0x41,
  0x2A,
  0x02,
  0x08,
  0x00,
  0x82,
  0x01,
  0x04,
  0x12,
  0x41,
  0x04,
  0x80,
};
const struct IrCode code_051Code PROGMEM = {
  freq_to_timerval(38462),
  52,		// # of pairs
  3,		// # of bits per index
  code_051Times,
  code_051Codes
};// Duplicate IR Code? - Similar to NA115

const uint16_t code_052Times[] PROGMEM = {
  16, 838,
  17, 558,
  17, 839,
  17, 6328,
};
const uint8_t code_052Codes[] PROGMEM = {
  0x1A,
  0x9A,
  0x9B,
  0x9A,
  0x9A,
  0x99,
};
const struct IrCode code_052Code PROGMEM = {
  freq_to_timerval(31250),
  24,		// # of pairs
  2,		// # of bits per index
  code_052Times,
  code_052Codes
};// Duplicate IR Code? -  Similar to EU017


/* Duplicate timing table, same as eu046 !
 const uint16_t code_053Times[] PROGMEM = {
 	15, 493,
 	16, 493,
 	16, 698,
 	16, 1414,
 };
 */
const uint8_t code_053Codes[] PROGMEM = {
  0x26,
  0xAB,
  0x66,
  0xAA,
};
const struct IrCode code_053Code PROGMEM = {
  freq_to_timerval(34483),
  16,		// # of pairs
  2,		// # of bits per index
  code_046Times,
  code_053Codes
};
const uint16_t code_054Times[] PROGMEM = {
  49, 53,
  49, 104,
  49, 262,
  49, 264,
  49, 8030,
  100, 103,
};
const uint8_t code_054Codes[] PROGMEM = {
  0x40,
  0x1A,
  0x23,
  0x00,
  0xD0,
  0x80,
};
const struct IrCode code_054Code PROGMEM = {
  freq_to_timerval(31250),
  14,		// # of pairs
  3,		// # of bits per index
  code_054Times,
  code_054Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_055Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_055Codes[] PROGMEM = {
  0x80,
  0x00,
  0x00,
  0x20,
  0x92,
  0x49,
  0x00,
  0x02,
  0x40,
  0x04,
  0x90,
  0x09,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_055Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_055Codes
};
const uint16_t code_056Times[] PROGMEM = {
  112, 107,
  113, 107,
  677, 2766,
};
const uint8_t code_056Codes[] PROGMEM = {
  0x26,
};
const struct IrCode code_056Code PROGMEM = {
  freq_to_timerval(38462),
  4,		// # of pairs
  2,		// # of bits per index
  code_056Times,
  code_056Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_057Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
/*
const uint8_t code_057Codes[] PROGMEM = {
 	0xA0,
 	0x00,
 	0x41,
 	0x04,
 	0x92,
 	0x08,
 	0x20,
 	0x02,
 	0x00,
 	0x04,
 	0x90,
 	0x49,
 	0x2B,
 	0x3D,
 	0x00,
 };
 const struct IrCode code_057Code PROGMEM = {
 	freq_to_timerval(38462),
 	38,		// # of pairs
 	3,		// # of bits per index
 	code_004Times,
 	code_057Codes
 }; // Duplicate IR code - same as EU008
 */
/* Duplicate timing table, same as na009 !
 const uint16_t code_058Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_058Codes[] PROGMEM = {
  0x80,
  0x00,
  0x00,
  0x24,
  0x10,
  0x49,
  0x00,
  0x82,
  0x00,
  0x04,
  0x10,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_058Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_058Codes
};
const uint16_t code_059Times[] PROGMEM = {
  310, 613,
  310, 614,
  622, 8312,
};
const uint8_t code_059Codes[] PROGMEM = {
  0x26,
};
const struct IrCode code_059Code PROGMEM = {
  freq_to_timerval(41667),
  4,		// # of pairs
  2,		// # of bits per index
  code_059Times,
  code_059Codes
};// Duplicate IR Code? - Similar to EU056

const uint16_t code_060Times[] PROGMEM = {
  50, 158,
  53, 51,
  53, 156,
  53, 2180,
};
const uint8_t code_060Codes[] PROGMEM = {
  0x25,
  0x59,
  0x9A,
  0x5A,
  0xE9,
  0x56,
  0x66,
  0x96,
  0xA0,
};
const struct IrCode code_060Code PROGMEM = {
  freq_to_timerval(38462),
  34,		// # of pairs
  2,		// # of bits per index
  code_060Times,
  code_060Codes
};

/* Duplicate timing table, same as na005 !
 const uint16_t code_061Times[] PROGMEM = {
 	88, 90,
 	88, 91,
 	88, 181,
 	88, 8976,
 	177, 91,
 };
 */
const uint8_t code_061Codes[] PROGMEM = {
  0x10,
  0x92,
  0x54,
  0x24,
  0xB3,
  0x09,
  0x25,
  0x42,
  0x48,
};
const struct IrCode code_061Code PROGMEM = {
  freq_to_timerval(35714),
  24,		// # of pairs
  3,		// # of bits per index
  code_005Times,
  code_061Codes
};

/* Duplicate timing table, same as eu060 !
 const uint16_t code_062Times[] PROGMEM = {
 	50, 158,
 	53, 51,
 	53, 156,
 	53, 2180,
 };
 */
const uint8_t code_062Codes[] PROGMEM = {
  0x25,
  0x99,
  0x9A,
  0x5A,
  0xE9,
  0x66,
  0x66,
  0x96,
  0xA0,
};
const struct IrCode code_062Code PROGMEM = {
  freq_to_timerval(38462),
  34,		// # of pairs
  2,		// # of bits per index
  code_060Times,
  code_062Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_063Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_063Codes[] PROGMEM = {
  0x80,
  0x00,
  0x00,
  0x24,
  0x90,
  0x41,
  0x00,
  0x82,
  0x00,
  0x04,
  0x10,
  0x49,
  0x2A,
  0xBA,
  0x00,
};
const struct IrCode code_063Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_063Codes
};
const uint16_t code_064Times[] PROGMEM = {
  47, 267,
  50, 55,
  50, 110,
  50, 265,
  50, 2055,
  50, 12117,
  100, 57,
  100, 112,
};
const uint8_t code_064Codes[] PROGMEM = {
  0x04,
  0x92,
  0x49,
  0x26,
  0x32,
  0x51,
  0xCB,
  0xD6,
  0x4A,
  0x39,
  0x72,
};
const struct IrCode code_064Code PROGMEM = {
  freq_to_timerval(30395),
  29,		// # of pairs
  3,		// # of bits per index
  code_064Times,
  code_064Codes
};
const uint16_t code_065Times[] PROGMEM = {
  47, 267,
  50, 55,
  50, 110,
  50, 265,
  50, 2055,
  50, 12117,
  100, 112,
};
const uint8_t code_065Codes[] PROGMEM = {
  0x04,
  0x92,
  0x49,
  0x26,
  0x32,
  0x4A,
  0x38,
  0x9A,
  0xC9,
  0x28,
  0xE2,
  0x48,
};
const struct IrCode code_065Code PROGMEM = {
  freq_to_timerval(30303),
  31,		// # of pairs
  3,		// # of bits per index
  code_065Times,
  code_065Codes
};

/* Duplicate timing table, same as eu049 !
 const uint16_t code_066Times[] PROGMEM = {
 	55, 55,
 	55, 167,
 	55, 4577,
 	55, 9506,
 	448, 445,
 	450, 444,
 };
 */
const uint8_t code_066Codes[] PROGMEM = {
  0x84,
  0x82,
  0x00,
  0x04,
  0x82,
  0x00,
  0x00,
  0x82,
  0x00,
  0x04,
  0x10,
  0x49,
  0x2A,
  0x87,
  0x41,
};
const struct IrCode code_066Code PROGMEM = {
  freq_to_timerval(38462),
  40,		// # of pairs
  3,		// # of bits per index
  code_049Times,
  code_066Codes
};
const uint16_t code_067Times[] PROGMEM = {
  94, 473,
  94, 728,
  102, 1637,
};
const uint8_t code_067Codes[] PROGMEM = {
  0x41,
  0x24,
  0x12,
};
const struct IrCode code_067Code PROGMEM = {
  freq_to_timerval(38462),
  12,		// # of pairs
  2,		// # of bits per index
  code_067Times,
  code_067Codes
};
const uint16_t code_068Times[] PROGMEM = {
  49, 263,
  50, 54,
  50, 108,
  50, 263,
  50, 2029,
  50, 10199,
  100, 110,
};
const uint8_t code_068Codes[] PROGMEM = {
  0x04,
  0x92,
  0x49,
  0x26,
  0x34,
  0x49,
  0x38,
  0x9A,
  0xD1,
  0x24,
  0xE2,
  0x48,
};
const struct IrCode code_068Code PROGMEM = {
  freq_to_timerval(38610),
  31,		// # of pairs
  3,		// # of bits per index
  code_068Times,
  code_068Codes
};
const uint16_t code_069Times[] PROGMEM = {
  4, 499,
  4, 750,
  4, 4999,
};
const uint8_t code_069Codes[] PROGMEM = {
  0x05,
  0x54,
  0x06,
  0x05,
  0x54,
  0x04,
};
const struct IrCode code_069Code PROGMEM = {
  0,              // Non-pulsed code
  23,		// # of pairs
  2,		// # of bits per index
  code_069Times,
  code_069Codes
};

/* Duplicate timing table, same as eu069 !
 const uint16_t code_070Times[] PROGMEM = {
 	4, 499,
 	4, 750,
 	4, 4999,
 };
 */
const uint8_t code_070Codes[] PROGMEM = {
  0x14,
  0x54,
  0x06,
  0x14,
  0x54,
  0x04,
};
const struct IrCode code_070Code PROGMEM = {
  0,              // Non-pulsed code
  23,		// # of pairs
  2,		// # of bits per index
  code_069Times,
  code_070Codes
};
const uint16_t code_071Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 4422,
};
const uint8_t code_071Codes[] PROGMEM = {
  0x45,
  0x44,
  0x56,
  0x45,
  0x44,
  0x55,
};
const struct IrCode code_071Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_071Times,
  code_071Codes
};
const uint16_t code_072Times[] PROGMEM = {
  5, 568,
  5, 854,
  5, 4999,
};
const uint8_t code_072Codes[] PROGMEM = {
  0x55,
  0x45,
  0x46,
  0x55,
  0x45,
  0x44,
};
const struct IrCode code_072Code PROGMEM = {
  0,              // Non-pulsed code
  23,		// # of pairs
  2,		// # of bits per index
  code_072Times,
  code_072Codes
};

/* Duplicate timing table, same as eu046 !
 const uint16_t code_073Times[] PROGMEM = {
 	15, 493,
 	16, 493,
 	16, 698,
 	16, 1414,
 };
 */
const uint8_t code_073Codes[] PROGMEM = {
  0x19,
  0x57,
  0x59,
  0x55,
};
const struct IrCode code_073Code PROGMEM = {
  freq_to_timerval(34483),
  16,		// # of pairs
  2,		// # of bits per index
  code_046Times,
  code_073Codes
};

/* Duplicate timing table, same as na031 !
 const uint16_t code_074Times[] PROGMEM = {
 	88, 89,
 	88, 90,
 	88, 179,
 	88, 8977,
 	177, 90,
 };
 */
const uint8_t code_074Codes[] PROGMEM = {
  0x04,
  0x92,
  0x49,
  0x28,
  0xC6,
  0x49,
  0x24,
  0x92,
  0x51,
  0x80,
};
const struct IrCode code_074Code PROGMEM = {
  freq_to_timerval(35714),
  26,		// # of pairs
  3,		// # of bits per index
  code_031Times,
  code_074Codes
};
const uint16_t code_075Times[] PROGMEM = {
  6, 566,
  6, 851,
  6, 5474,
};
const uint8_t code_075Codes[] PROGMEM = {
  0x05,
  0x45,
  0x46,
  0x05,
  0x45,
  0x44,
};
const struct IrCode code_075Code PROGMEM = {
  0,              // Non-pulsed code
  23,		// # of pairs
  2,		// # of bits per index
  code_075Times,
  code_075Codes
};
const uint16_t code_076Times[] PROGMEM = {
  14, 843,
  16, 555,
  16, 841,
  16, 4911,
};
const uint8_t code_076Codes[] PROGMEM = {
  0x2A,
  0x9A,
  0x9B,
  0xAA,
  0x9A,
  0x9A,
};
const struct IrCode code_076Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_076Times,
  code_076Codes
};

/* Duplicate timing table, same as eu028 !
 const uint16_t code_077Times[] PROGMEM = {
 	47, 267,
 	50, 55,
 	50, 110,
 	50, 265,
 	50, 2055,
 	50, 12117,
 	100, 57,
 };
 */
const uint8_t code_077Codes[] PROGMEM = {
  0x04,
  0x92,
  0x49,
  0x26,
  0x32,
  0x51,
  0xC8,
  0x9A,
  0xC9,
  0x47,
  0x22,
  0x48,
};
const struct IrCode code_077Code PROGMEM = {
  freq_to_timerval(30303),
  31,		// # of pairs
  3,		// # of bits per index
  code_028Times,
  code_077Codes
};
const uint16_t code_078Times[] PROGMEM = {
  6, 925,
  6, 1339,
  6, 2098,
  6, 2787,
};
const uint8_t code_078Codes[] PROGMEM = {
  0x90,
  0x0D,
  0x00,
};
const struct IrCode code_078Code PROGMEM = {
  0,              // Non-pulsed code
  12,		// # of pairs
  2,		// # of bits per index
  code_078Times,
  code_078Codes
};
const uint16_t code_079Times[] PROGMEM = {
  53, 59,
  53, 170,
  53, 4359,
  892, 448,
  893, 448,
};
const uint8_t code_079Codes[] PROGMEM = {
  0x60,
  0x00,
  0x00,
  0x24,
  0x80,
  0x09,
  0x04,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0x00,
  0x00,
  0x00,
  0x92,
  0x00,
  0x24,
  0x12,
  0x48,
  0x00,
  0x00,
  0x01,
  0x24,
  0x80,
};
const struct IrCode code_079Code PROGMEM = {
  freq_to_timerval(38462),
  68,		// # of pairs
  3,		// # of bits per index
  code_079Times,
  code_079Codes
};
const uint16_t code_080Times[] PROGMEM = {
  55, 57,
  55, 167,
  55, 4416,
  895, 448,
  897, 447,
};
const uint8_t code_080Codes[] PROGMEM = {
  0x60,
  0x00,
  0x00,
  0x20,
  0x10,
  0x09,
  0x04,
  0x02,
  0x01,
  0x00,
  0x90,
  0x48,
  0x2A,
  0x00,
  0x00,
  0x00,
  0x80,
  0x40,
  0x24,
  0x10,
  0x08,
  0x04,
  0x02,
  0x41,
  0x20,
  0x80,
};
const struct IrCode code_080Code PROGMEM = {
  freq_to_timerval(38462),
  68,		// # of pairs
  3,		// # of bits per index
  code_080Times,
  code_080Codes
};

const uint16_t code_081Times[] PROGMEM = {
  26, 185,
  27, 80,
  27, 185,
  27, 4249,
};
const uint8_t code_081Codes[] PROGMEM = {
  0x1A,
  0x5A,
  0x65,
  0x67,
  0x9A,
  0x65,
  0x9A,
  0x9B,
  0x9A,
  0x5A,
  0x65,
  0x67,
  0x9A,
  0x65,
  0x9A,
  0x9B,
  0x9A,
  0x5A,
  0x65,
  0x65,
};
const struct IrCode code_081Code PROGMEM = {
  freq_to_timerval(38462),
  80,		// # of pairs
  2,		// # of bits per index
  code_081Times,
  code_081Codes
};
const uint16_t code_082Times[] PROGMEM = {
  51, 56,
  51, 162,
  51, 2842,
  848, 430,
  850, 429,
};
const uint8_t code_082Codes[] PROGMEM = {
  0x60,
  0x82,
  0x08,
  0x24,
  0x10,
  0x41,
  0x04,
  0x82,
  0x40,
  0x00,
  0x10,
  0x09,
  0x2A,
  0x02,
  0x08,
  0x20,
  0x90,
  0x41,
  0x04,
  0x12,
  0x09,
  0x00,
  0x00,
  0x40,
  0x24,
  0x80,
};
const struct IrCode code_082Code PROGMEM = {
  freq_to_timerval(40000),
  68,		// # of pairs
  3,		// # of bits per index
  code_082Times,
  code_082Codes
};
const uint16_t code_083Times[] PROGMEM = {
  16, 559,
  16, 847,
  16, 5900,
  17, 559,
  17, 847,
};
const uint8_t code_083Codes[] PROGMEM = {
  0x0E,
  0x38,
  0x21,
  0x82,
  0x26,
  0x20,
  0x82,
  0x48,
  0x23,
};
const struct IrCode code_083Code PROGMEM = {
  freq_to_timerval(33333),
  24,		// # of pairs
  3,		// # of bits per index
  code_083Times,
  code_083Codes
};
const uint16_t code_084Times[] PROGMEM = {
  16, 484,
  16, 738,
  16, 739,
  16, 4795,
};
const uint8_t code_084Codes[] PROGMEM = {
  0x6A,
  0xA0,
  0x03,
  0xAA,
  0xA0,
  0x01,
};
const struct IrCode code_084Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_084Times,
  code_084Codes
};
const uint16_t code_085Times[] PROGMEM = {
  48, 52,
  48, 160,
  48, 400,
  48, 2120,
  799, 400,
};
const uint8_t code_085Codes[] PROGMEM = {
  0x84,
  0x82,
  0x40,
  0x08,
  0x92,
  0x48,
  0x01,
  0xC2,
  0x41,
  0x20,
  0x04,
  0x49,
  0x24,
  0x00,
  0x40,
};
const struct IrCode code_085Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_085Times,
  code_085Codes
};
const uint16_t code_086Times[] PROGMEM = {
  16, 851,
  17, 554,
  17, 850,
  17, 851,
  17, 4847,
};
const uint8_t code_086Codes[] PROGMEM = {
  0x45,
  0x86,
  0x5B,
  0x05,
  0xC6,
  0x5B,
  0x05,
  0xB0,
  0x42,
};
const struct IrCode code_086Code PROGMEM = {
  freq_to_timerval(33333),
  24,		// # of pairs
  3,		// # of bits per index
  code_086Times,
  code_086Codes
};
const uint16_t code_087Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 5126,
};
const uint8_t code_087Codes[] PROGMEM = {
  0x55,
  0x50,
  0x02,
  0x55,
  0x50,
  0x01,
};
const struct IrCode code_087Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_087Times,
  code_087Codes
};
const uint16_t code_088Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 4874,
};
const uint8_t code_088Codes[] PROGMEM = {
  0x45,
  0x54,
  0x42,
  0x45,
  0x54,
  0x41,
};
const struct IrCode code_088Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_088Times,
  code_088Codes
};

/* Duplicate timing table, same as na021 !
 const uint16_t code_089Times[] PROGMEM = {
 	48, 52,
 	48, 160,
 	48, 400,
 	48, 2335,
 	799, 400,
 };
 */
const uint8_t code_089Codes[] PROGMEM = {
  0x84,
  0x10,
  0x40,
  0x08,
  0x82,
  0x08,
  0x01,
  0xC2,
  0x08,
  0x20,
  0x04,
  0x41,
  0x04,
  0x00,
  0x40,
};
const struct IrCode code_089Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_021Times,
  code_089Codes
};
const uint16_t code_090Times[] PROGMEM = {
  3, 9,
  3, 19,
  3, 29,
  3, 39,
  3, 9968,
};
const uint8_t code_090Codes[] PROGMEM = {
  0x60,
  0x00,
  0x88,
  0x00,
  0x02,
  0xE3,
  0x00,
  0x04,
  0x40,
  0x00,
  0x16,
};
const struct IrCode code_090Code PROGMEM = {
  0,              // Non-pulsed code
  29,		// # of pairs
  3,		// # of bits per index
  code_090Times,
  code_090Codes
};
const uint16_t code_091Times[] PROGMEM = {
  15, 138,
  15, 446,
  15, 605,
  15, 6565,
};
const uint8_t code_091Codes[] PROGMEM = {
  0x80,
  0x01,
  0x00,
  0x2E,
  0x00,
  0x04,
  0x00,
  0xA0,
};
const struct IrCode code_091Code PROGMEM = {
  freq_to_timerval(38462),
  30,		// # of pairs
  2,		// # of bits per index
  code_091Times,
  code_091Codes
};
const uint16_t code_092Times[] PROGMEM = {
  48, 50,
  48, 148,
  48, 149,
  48, 1424,
};
const uint8_t code_092Codes[] PROGMEM = {
  0x48,
  0x80,
  0x0E,
  0x22,
  0x00,
  0x10,
};
const struct IrCode code_092Code PROGMEM = {
  freq_to_timerval(40000),
  22,		// # of pairs
  2,		// # of bits per index
  code_092Times,
  code_092Codes
};
const uint16_t code_093Times[] PROGMEM = {
  87, 639,
  88, 275,
  88, 639,
};
const uint8_t code_093Codes[] PROGMEM = {
  0x15,
  0x9A,
  0x94,
};
const struct IrCode code_093Code PROGMEM = {
  freq_to_timerval(35714),
  11,		// # of pairs
  2,		// # of bits per index
  code_093Times,
  code_093Codes
};
const uint16_t code_094Times[] PROGMEM = {
  3, 8,
  3, 18,
  3, 24,
  3, 38,
  3, 9969,
};
const uint8_t code_094Codes[] PROGMEM = {
  0x60,
  0x80,
  0x88,
  0x00,
  0x00,
  0xE3,
  0x04,
  0x04,
  0x40,
  0x00,
  0x06,
};
const struct IrCode code_094Code PROGMEM = {
  0,              // Non-pulsed code
  29,		// # of pairs
  3,		// # of bits per index
  code_094Times,
  code_094Codes
};

/* Duplicate timing table, same as eu046 !
 const uint16_t code_095Times[] PROGMEM = {
 	15, 493,
 	16, 493,
 	16, 698,
 	16, 1414,
 };
 */
const uint8_t code_095Codes[] PROGMEM = {
  0x2A,
  0xAB,
  0x6A,
  0xAA,
};
const struct IrCode code_095Code PROGMEM = {
  freq_to_timerval(34483),
  16,		// # of pairs
  2,		// # of bits per index
  code_046Times,
  code_095Codes
};
const uint16_t code_096Times[] PROGMEM = {
  13, 608,
  14, 141,
  14, 296,
  14, 451,
  14, 606,
  14, 608,
  14, 6207,
};
const uint8_t code_096Codes[] PROGMEM = {
  0x04,
  0x94,
  0x4B,
  0x24,
  0x95,
  0x35,
  0x24,
  0xA2,
  0x59,
  0x24,
  0xA8,
  0x40,
};
const struct IrCode code_096Code PROGMEM = {
  freq_to_timerval(38462),
  30,		// # of pairs
  3,		// # of bits per index
  code_096Times,
  code_096Codes
};

/* Duplicate timing table, same as eu046 !
 const uint16_t code_097Times[] PROGMEM = {
 	15, 493,
 	16, 493,
 	16, 698,
 	16, 1414,
 };
 */
const uint8_t code_097Codes[] PROGMEM = {
  0x19,
  0xAB,
  0x59,
  0xA9,
};
const struct IrCode code_097Code PROGMEM = {
  freq_to_timerval(34483),
  16,		// # of pairs
  2,		// # of bits per index
  code_046Times,
  code_097Codes
};
const uint16_t code_098Times[] PROGMEM = {
  3, 8,
  3, 18,
  3, 28,
  3, 12731,
};
const uint8_t code_098Codes[] PROGMEM = {
  0x80,
  0x01,
  0x00,
  0xB8,
  0x55,
  0x10,
  0x08,
};
const struct IrCode code_098Code PROGMEM = {
  0,              // Non-pulsed code
  27,		// # of pairs
  2,		// # of bits per index
  code_098Times,
  code_098Codes
};
const uint16_t code_099Times[] PROGMEM = {
  46, 53,
  46, 106,
  46, 260,
  46, 1502,
  46, 10962,
  93, 53,
  93, 106,
};
const uint8_t code_099Codes[] PROGMEM = {
  0x46,
  0x80,
  0x00,
  0x00,
  0x00,
  0x03,
  0x44,
  0x52,
  0x00,
  0x00,
  0x0C,
  0x22,
  0x22,
  0x90,
  0x00,
  0x00,
  0x60,
  0x80,
};
const struct IrCode code_099Code PROGMEM = {
  freq_to_timerval(35714),
  46,		// # of pairs
  3,		// # of bits per index
  code_099Times,
  code_099Codes
};


/* Duplicate timing table, same as eu098 !
 const uint16_t code_100Times[] PROGMEM = {
 	3, 8,
 	3, 18,
 	3, 28,
 	3, 12731,
 };
 */
const uint8_t code_100Codes[] PROGMEM = {
  0x80,
  0x04,
  0x00,
  0xB8,
  0x55,
  0x40,
  0x08,
};
const struct IrCode code_100Code PROGMEM = {
  0,              // Non-pulsed code
  27,		// # of pairs
  2,		// # of bits per index
  code_098Times,
  code_100Codes
};



const uint16_t code_101Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 4674,
};
const uint8_t code_101Codes[] PROGMEM = {
  0x55,
  0x50,
  0x06,
  0x55,
  0x50,
  0x05,
};
const struct IrCode code_101Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_101Times,
  code_101Codes
};

/* Duplicate timing table, same as eu087 !
 const uint16_t code_102Times[] PROGMEM = {
 	14, 491,
 	14, 743,
 	14, 5126,
 };
 */
const uint8_t code_102Codes[] PROGMEM = {
  0x45,
  0x54,
  0x02,
  0x45,
  0x54,
  0x01,
};
const struct IrCode code_102Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_087Times,
  code_102Codes
};
const uint16_t code_103Times[] PROGMEM = {
  44, 815,
  45, 528,
  45, 815,
  45, 5000,
};
const uint8_t code_103Codes[] PROGMEM = {
  0x29,
  0x9A,
  0x9B,
  0xA9,
  0x9A,
  0x9A,
};
const struct IrCode code_103Code PROGMEM = {
  freq_to_timerval(34483),
  24,		// # of pairs
  2,		// # of bits per index
  code_103Times,
  code_103Codes
};
const uint16_t code_104Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 5881,
};
const uint8_t code_104Codes[] PROGMEM = {
  0x44,
  0x40,
  0x02,
  0x44,
  0x40,
  0x01,
};
const struct IrCode code_104Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_104Times,
  code_104Codes
};

/* Duplicate timing table, same as na009 !
 const uint16_t code_105Times[] PROGMEM = {
 	53, 56,
 	53, 171,
 	53, 3950,
 	53, 9599,
 	898, 451,
 	900, 226,
 };
 */
const uint8_t code_105Codes[] PROGMEM = {
  0x84,
  0x10,
  0x00,
  0x20,
  0x90,
  0x01,
  0x00,
  0x80,
  0x40,
  0x04,
  0x12,
  0x09,
  0x2A,
  0xBA,
  0x40,
};
const struct IrCode code_105Code PROGMEM = {
  freq_to_timerval(38610),
  38,		// # of pairs
  3,		// # of bits per index
  code_009Times,
  code_105Codes
};
const uint16_t code_106Times[] PROGMEM = {
  48, 246,
  50, 47,
  50, 94,
  50, 245,
  50, 1488,
  50, 10970,
  100, 47,
  100, 94,
};
const uint8_t code_106Codes[] PROGMEM = {
  0x0B,
  0x12,
  0x49,
  0x24,
  0x92,
  0x49,
  0x8D,
  0x1C,
  0x89,
  0x27,
  0xFC,
  0xAB,
  0x47,
  0x22,
  0x49,
  0xFF,
  0x2A,
  0xD1,
  0xC8,
  0x92,
  0x7F,
  0xC9,
  0x00,
};
const struct IrCode code_106Code PROGMEM = {
  freq_to_timerval(38462),
  59,		// # of pairs
  3,		// # of bits per index
  code_106Times,
  code_106Codes
};
const uint16_t code_107Times[] PROGMEM = {
  16, 847,
  16, 5900,
  17, 559,
  17, 846,
  17, 847,
};
const uint8_t code_107Codes[] PROGMEM = {
  0x62,
  0x08,
  0xA0,
  0x8A,
  0x19,
  0x04,
  0x08,
  0x40,
  0x83,
};
const struct IrCode code_107Code PROGMEM = {
  freq_to_timerval(33333),
  24,		// # of pairs
  3,		// # of bits per index
  code_107Times,
  code_107Codes
};
const uint16_t code_108Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 4622,
};
const uint8_t code_108Codes[] PROGMEM = {
  0x45,
  0x54,
  0x16,
  0x45,
  0x54,
  0x15,
};
const struct IrCode code_108Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_108Times,
  code_108Codes
};
const uint16_t code_109Times[] PROGMEM = {
  24, 185,
  27, 78,
  27, 183,
  27, 1542,
};
const uint8_t code_109Codes[] PROGMEM = {
  0x19,
  0x95,
  0x5E,
  0x66,
  0x55,
  0x50,
};
const struct IrCode code_109Code PROGMEM = {
  freq_to_timerval(38462),
  22,		// # of pairs
  2,		// # of bits per index
  code_109Times,
  code_109Codes
};


const uint16_t code_110Times[] PROGMEM = {
  56, 55,
  56, 168,
  56, 4850,
  447, 453,
  448, 453,
};
const uint8_t code_110Codes[] PROGMEM = {
  0x64,
  0x10,
  0x00,
  0x04,
  0x10,
  0x00,
  0x00,
  0x80,
  0x00,
  0x04,
  0x12,
  0x49,
  0x2A,
  0x10,
  0x40,
  0x00,
  0x10,
  0x40,
  0x00,
  0x02,
  0x00,
  0x00,
  0x10,
  0x49,
  0x24,
  0x90,
};
const struct IrCode code_110Code PROGMEM = {
  freq_to_timerval(38462),
  68,		// # of pairs
  3,		// # of bits per index
  code_110Times,
  code_110Codes
};
const uint16_t code_111Times[] PROGMEM = {
  49, 52,
  49, 250,
  49, 252,
  49, 2377,
  49, 12009,
  100, 52,
  100, 102,
};
const uint8_t code_111Codes[] PROGMEM = {
  0x22,
  0x80,
  0x1A,
  0x18,
  0x01,
  0x10,
  0xC0,
  0x02,
};
const struct IrCode code_111Code PROGMEM = {
  freq_to_timerval(31250),
  21,		// # of pairs
  3,		// # of bits per index
  code_111Times,
  code_111Codes
};
const uint16_t code_112Times[] PROGMEM = {
  55, 55,
  55, 167,
  55, 5023,
  55, 9506,
  448, 445,
  450, 444,
};
const uint8_t code_112Codes[] PROGMEM = {
  0x80,
  0x02,
  0x00,
  0x00,
  0x02,
  0x00,
  0x04,
  0x92,
  0x00,
  0x00,
  0x00,
  0x49,
  0x2A,
  0x97,
  0x48,
};
const struct IrCode code_112Code PROGMEM = {
  freq_to_timerval(38462),
  40,		// # of pairs
  3,		// # of bits per index
  code_112Times,
  code_112Codes
};


/* Duplicate timing table, same as eu054 !
 const uint16_t code_113Times[] PROGMEM = {
 	49, 53,
 	49, 104,
 	49, 262,
 	49, 264,
 	49, 8030,
 	100, 103,
 };
 */
const uint8_t code_113Codes[] PROGMEM = {
  0x46,
  0x80,
  0x23,
  0x34,
  0x00,
  0x80,
};
const struct IrCode code_113Code PROGMEM = {
  freq_to_timerval(31250),
  14,		// # of pairs
  3,		// # of bits per index
  code_054Times,
  code_113Codes
};

/* Duplicate timing table, same as eu028 !
 const uint16_t code_114Times[] PROGMEM = {
 	47, 267,
 	50, 55,
 	50, 110,
 	50, 265,
 	50, 2055,
 	50, 12117,
 	100, 57,
 };
 */
const uint8_t code_114Codes[] PROGMEM = {
  0x04,
  0x92,
  0x49,
  0x26,
  0x34,
  0x71,
  0x44,
  0x9A,
  0xD1,
  0xC5,
  0x12,
  0x48,
};
const struct IrCode code_114Code PROGMEM = {
  freq_to_timerval(30303),
  31,		// # of pairs
  3,		// # of bits per index
  code_028Times,
  code_114Codes
};


const uint16_t code_115Times[] PROGMEM = {
  48, 98,
  48, 196,
  97, 836,
  395, 388,
  1931, 389,
};
const uint8_t code_115Codes[] PROGMEM = {
  0x84,
  0x92,
  0x01,
  0x24,
  0x12,
  0x00,
  0x04,
  0x80,
  0x08,
  0x09,
  0x92,
  0x48,
  0x04,
  0x90,
  0x48,
  0x00,
  0x12,
  0x00,
  0x20,
  0x26,
  0x49,
  0x20,
  0x12,
  0x41,
  0x20,
  0x00,
  0x48,
  0x00,
  0x82,
};
const struct IrCode code_115Code PROGMEM = {
  freq_to_timerval(58824),
  77,		// # of pairs
  3,		// # of bits per index
  code_115Times,
  code_115Codes
};
const uint16_t code_116Times[] PROGMEM = {
  3, 9,
  3, 31,
  3, 42,
  3, 10957,
};
const uint8_t code_116Codes[] PROGMEM = {
  0x80,
  0x01,
  0x00,
  0x2E,
  0x00,
  0x04,
  0x00,
  0x80,
};
const struct IrCode code_116Code PROGMEM = {
  0,              // Non-pulsed code
  29,		// # of pairs
  2,		// # of bits per index
  code_116Times,
  code_116Codes
};
const uint16_t code_117Times[] PROGMEM = {
  49, 53,
  49, 262,
  49, 264,
  49, 8030,
  100, 103,
};
const uint8_t code_117Codes[] PROGMEM = {
  0x22,
  0x00,
  0x1A,
  0x10,
  0x00,
  0x40,
};
const struct IrCode code_117Code PROGMEM = {
  freq_to_timerval(31250),
  14,		// # of pairs
  3,		// # of bits per index
  code_117Times,
  code_117Codes
};
const uint16_t code_118Times[] PROGMEM = {
  44, 815,
  45, 528,
  45, 815,
  45, 4713,
};
const uint8_t code_118Codes[] PROGMEM = {
  0x2A,
  0x9A,
  0x9B,
  0xAA,
  0x9A,
  0x9A,
};
const struct IrCode code_118Code PROGMEM = {
  freq_to_timerval(34483),
  24,		// # of pairs
  2,		// # of bits per index
  code_118Times,
  code_118Codes
};

const uint16_t code_119Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 5430,
};
const uint8_t code_119Codes[] PROGMEM = {
  0x44,
  0x44,
  0x02,
  0x44,
  0x44,
  0x01,
};
const struct IrCode code_119Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_119Times,
  code_119Codes
};


const uint16_t code_120Times[] PROGMEM = {
  19, 78,
  21, 27,
  21, 77,
  21, 3785,
  22, 0,
};
const uint8_t code_120Codes[] PROGMEM = {
  0x09,
  0x24,
  0x92,
  0x49,
  0x12,
  0x4A,
  0x24,
  0x92,
  0x49,
  0x24,
  0x92,
  0x49,
  0x24,
  0x94,
  0x89,
  0x69,
  0x24,
  0x92,
  0x49,
  0x22,
  0x49,
  0x44,
  0x92,
  0x49,
  0x24,
  0x92,
  0x49,
  0x24,
  0x92,
  0x91,
  0x30,
};
const struct IrCode code_120Code PROGMEM = {
  freq_to_timerval(38462),
  82,		// # of pairs
  3,		// # of bits per index
  code_120Times,
  code_120Codes
};

/* Duplicate timing table, same as eu051 !
 const uint16_t code_121Times[] PROGMEM = {
 	84, 88,
 	84, 261,
 	84, 3360,
 	347, 347,
 	347, 348,
 };
 */
const uint8_t code_121Codes[] PROGMEM = {
  0x64,
  0x00,
  0x09,
  0x24,
  0x00,
  0x09,
  0x24,
  0x00,
  0x09,
  0x2A,
  0x10,
  0x00,
  0x24,
  0x90,
  0x00,
  0x24,
  0x90,
  0x00,
  0x24,
  0x90,
};
const struct IrCode code_121Code PROGMEM = {
  freq_to_timerval(38462),
  52,		// # of pairs
  3,		// # of bits per index
  code_051Times,
  code_121Codes
};

/* Duplicate timing table, same as eu120 !
 const uint16_t code_122Times[] PROGMEM = {
 	19, 78,
 	21, 27,
 	21, 77,
 	21, 3785,
 	22, 0,
 };
 */
const uint8_t code_122Codes[] PROGMEM = {
  0x04,
  0xA4,
  0x92,
  0x49,
  0x22,
  0x49,
  0x48,
  0x92,
  0x49,
  0x24,
  0x92,
  0x49,
  0x24,
  0x94,
  0x89,
  0x68,
  0x94,
  0x92,
  0x49,
  0x24,
  0x49,
  0x29,
  0x12,
  0x49,
  0x24,
  0x92,
  0x49,
  0x24,
  0x92,
  0x91,
  0x30,
};
const struct IrCode code_122Code PROGMEM = {
  freq_to_timerval(38462),
  82,		// # of pairs
  3,		// # of bits per index
  code_120Times,
  code_122Codes
};
const uint16_t code_123Times[] PROGMEM = {
  13, 490,
  13, 741,
  13, 742,
  13, 5443,
};
const uint8_t code_123Codes[] PROGMEM = {
  0x6A,
  0xA0,
  0x0B,
  0xAA,
  0xA0,
  0x09,
};
const struct IrCode code_123Code PROGMEM = {
  freq_to_timerval(40000),
  24,		// # of pairs
  2,		// # of bits per index
  code_123Times,
  code_123Codes
};
const uint16_t code_124Times[] PROGMEM = {
  50, 54,
  50, 158,
  50, 407,
  50, 2153,
  843, 407,
};
const uint8_t code_124Codes[] PROGMEM = {
  0x80,
  0x10,
  0x40,
  0x08,
  0x92,
  0x48,
  0x01,
  0xC0,
  0x08,
  0x20,
  0x04,
  0x49,
  0x24,
  0x00,
  0x00,
};
const struct IrCode code_124Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_124Times,
  code_124Codes
};
const uint16_t code_125Times[] PROGMEM = {
  55, 56,
  55, 168,
  55, 3929,
  56, 0,
  882, 454,
  884, 452,
};
const uint8_t code_125Codes[] PROGMEM = {
  0x84,
  0x80,
  0x00,
  0x20,
  0x82,
  0x49,
  0x00,
  0x02,
  0x00,
  0x04,
  0x90,
  0x49,
  0x2A,
  0x92,
  0x00,
  0x00,
  0x82,
  0x09,
  0x24,
  0x00,
  0x08,
  0x00,
  0x12,
  0x41,
  0x24,
  0xB0,
};
const struct IrCode code_125Code PROGMEM = {
  freq_to_timerval(38462),
  68,		// # of pairs
  3,		// # of bits per index
  code_125Times,
  code_125Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_126Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_126Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x20,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_126Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_126Codes
};

/* Duplicate timing table, same as eu087 !
 const uint16_t code_127Times[] PROGMEM = {
 	14, 491,
 	14, 743,
 	14, 5126,
 };
 */
const uint8_t code_127Codes[] PROGMEM = {
  0x44,
  0x40,
  0x56,
  0x44,
  0x40,
  0x55,
};
const struct IrCode code_127Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_087Times,
  code_127Codes
};
const uint16_t code_128Times[] PROGMEM = {
  152, 471,
  154, 156,
  154, 469,
  154, 782,
  154, 2947,
};
const uint8_t code_128Codes[] PROGMEM = {
  0x05,
  0xC4,
  0x59,
};
const struct IrCode code_128Code PROGMEM = {
  freq_to_timerval(41667),
  8,		// # of pairs
  3,		// # of bits per index
  code_128Times,
  code_128Codes
};
const uint16_t code_129Times[] PROGMEM = {
  50, 50,
  50, 99,
  50, 251,
  50, 252,
  50, 1449,
  50, 11014,
  102, 49,
  102, 98,
};
const uint8_t code_129Codes[] PROGMEM = {
  0x47,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x8C,
  0x8C,
  0x40,
  0x03,
  0xF1,
  0xEB,
  0x23,
  0x10,
  0x00,
  0xFC,
  0x74,
};
const struct IrCode code_129Code PROGMEM = {
  freq_to_timerval(38462),
  45,		// # of pairs
  3,		// # of bits per index
  code_129Times,
  code_129Codes
};

/* Duplicate timing table, same as eu129 !
 const uint16_t code_130Times[] PROGMEM = {
 	50, 50,
 	50, 99,
 	50, 251,
 	50, 252,
 	50, 1449,
 	50, 11014,
 	102, 49,
 	102, 98,
 };
 */
const uint8_t code_130Codes[] PROGMEM = {
  0x47,
  0x00,
  0x00,
  0x00,
  0x00,
  0x00,
  0x8C,
  0x8C,
  0x40,
  0x03,
  0xE3,
  0xEB,
  0x23,
  0x10,
  0x00,
  0xF8,
  0xF4,
};
const struct IrCode code_130Code PROGMEM = {
  freq_to_timerval(38462),
  45,		// # of pairs
  3,		// # of bits per index
  code_129Times,
  code_130Codes
};
const uint16_t code_131Times[] PROGMEM = {
  14, 491,
  14, 743,
  14, 4170,
};
const uint8_t code_131Codes[] PROGMEM = {
  0x55,
  0x55,
  0x42,
  0x55,
  0x55,
  0x41,
};
const struct IrCode code_131Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_131Times,
  code_131Codes
};

/* Duplicate timing table, same as eu069 !
 const uint16_t code_132Times[] PROGMEM = {
 	4, 499,
 	4, 750,
 	4, 4999,
 };
 */
const uint8_t code_132Codes[] PROGMEM = {
  0x05,
  0x50,
  0x06,
  0x05,
  0x50,
  0x04,
};
const struct IrCode code_132Code PROGMEM = {
  0,              // Non-pulsed code
  23,		// # of pairs
  2,		// # of bits per index
  code_069Times,
  code_132Codes
};

/* Duplicate timing table, same as eu071 !
 const uint16_t code_133Times[] PROGMEM = {
 	14, 491,
 	14, 743,
 	14, 4422,
 };
 */
const uint8_t code_133Codes[] PROGMEM = {
  0x55,
  0x54,
  0x12,
  0x55,
  0x54,
  0x11,
};
const struct IrCode code_133Code PROGMEM = {
  freq_to_timerval(38462),
  24,		// # of pairs
  2,		// # of bits per index
  code_071Times,
  code_133Codes
};
const uint16_t code_134Times[] PROGMEM = {
  13, 490,
  13, 741,
  13, 742,
  13, 5939,
};
const uint8_t code_134Codes[] PROGMEM = {
  0x40,
  0x0A,
  0x83,
  0x80,
  0x0A,
  0x81,
};
const struct IrCode code_134Code PROGMEM = {
  freq_to_timerval(40000),
  24,		// # of pairs
  2,		// # of bits per index
  code_134Times,
  code_134Codes
};
const uint16_t code_135Times[] PROGMEM = {
  6, 566,
  6, 851,
  6, 5188,
};
const uint8_t code_135Codes[] PROGMEM = {
  0x54,
  0x45,
  0x46,
  0x54,
  0x45,
  0x44,
};
const struct IrCode code_135Code PROGMEM = {
  0,              // Non-pulsed code
  23,		// # of pairs
  2,		// # of bits per index
  code_135Times,
  code_135Codes
};

/* Duplicate timing table, same as na004 !
 const uint16_t code_136Times[] PROGMEM = {
 	55, 57,
 	55, 170,
 	55, 3949,
 	55, 9623,
 	56, 0,
 	898, 453,
 	900, 226,
 };
 */
const uint8_t code_136Codes[] PROGMEM = {
  0xA0,
  0x00,
  0x00,
  0x04,
  0x92,
  0x49,
  0x24,
  0x00,
  0x00,
  0x00,
  0x92,
  0x49,
  0x2B,
  0x3D,
  0x00,
};
const struct IrCode code_136Code PROGMEM = {
  freq_to_timerval(38462),
  38,		// # of pairs
  3,		// # of bits per index
  code_004Times,
  code_136Codes
};
const uint16_t code_137Times[] PROGMEM = {
  86, 91,
  87, 90,
  87, 180,
  87, 8868,
  88, 0,
  174, 90,
};
const uint8_t code_137Codes[] PROGMEM = {
  0x14,
  0x95,
  0x4A,
  0x35,
  0x9A,
  0x4A,
  0xA5,
  0x1B,
  0x00,
};
const struct IrCode code_137Code PROGMEM = {
  freq_to_timerval(35714),
  22,		// # of pairs
  3,		// # of bits per index
  code_137Times,
  code_137Codes
};
const uint16_t code_138Times[] PROGMEM = {
  4, 1036,
  4, 1507,
  4, 3005,
};
const uint8_t code_138Codes[] PROGMEM = {
  0x05,
  0x60,
  0x54,
};
const struct IrCode code_138Code PROGMEM = {
  0,              // Non-pulsed code
  11,		// # of pairs
  2,		// # of bits per index
  code_138Times,
  code_138Codes
};

const uint16_t code_139Times[] PROGMEM = {
  0, 0,
  14, 141,
  14, 452,
  14, 607,
  14, 6310,
};
const uint8_t code_139Codes[] PROGMEM = {
  0x64,
  0x92,
  0x4A,
  0x24,
  0x92,
  0xE3,
  0x24,
  0x92,
  0x51,
  0x24,
  0x96,
  0x00,
};
const struct IrCode code_139Code PROGMEM = {
  0,              // Non-pulsed code
  30,		// # of pairs
  3,		// # of bits per index
  code_139Times,
  code_139Codes
};

//Stupid GCC cares about where I put PROGMEM
//Should fix error: variable 'powerCodes' must be const in order to be put into read-only section by means of '__attribute__((progmem))' 
//const struct IrCode *powerCodes[] PROGMEM = {
const struct PROGMEM IrCode *powerCodes[] = {
	&code_000Code,
	&code_001Code,
	&code_002Code,
	&code_000Code, // same as &code_003Code
	&code_004Code,
	&code_005Code,
	&code_006Code,
	&code_007Code,
	&code_008Code,
	&code_005Code, // same as &code_009Code
	&code_004Code, // same as &code_010Code
	&code_011Code,
	&code_012Code,
	&code_013Code,
	&code_021Code, // same as &code_014Code
	&code_015Code,
	&code_016Code,
	&code_017Code,
	&code_018Code,
	&code_019Code,
	&code_020Code,
	&code_021Code,
	&code_022Code,
	&code_022Code, // same as &code_023Code
	&code_024Code,
	&code_025Code,
	&code_026Code,
	&code_027Code,
	&code_028Code,
	&code_029Code,
	&code_030Code,
	&code_031Code,
	&code_032Code,
	&code_033Code,
	&code_034Code,
	//&code_035Code, same as eu009
	&code_036Code,
	&code_037Code,
	&code_038Code,
	&code_039Code,
	&code_040Code,
	&code_041Code,
	&code_042Code,
	&code_043Code,
	&code_044Code,
	&code_045Code,
	&code_046Code,
	&code_047Code,
	&code_048Code,
	&code_049Code,
	&code_050Code,
	&code_051Code,
	&code_052Code,
	&code_053Code,
	&code_054Code,
	&code_055Code,
	&code_056Code,
	//&code_057Code, same as eu008
	&code_058Code,
	&code_059Code,
	&code_060Code,
	&code_061Code,
	&code_062Code,
	&code_063Code,
	&code_064Code,
	&code_065Code,
	&code_066Code,
	&code_067Code,
	&code_068Code,
	&code_069Code,
	&code_070Code,
	&code_071Code,
	&code_072Code,
	&code_073Code,
	&code_074Code,
	&code_075Code,
	&code_076Code,
	&code_077Code,
	&code_078Code,
	&code_079Code,
	&code_080Code,
	&code_081Code,
	&code_082Code,
	&code_083Code,
	&code_084Code,
	&code_085Code,
	&code_086Code,
	&code_087Code,
	&code_088Code,
	&code_089Code,
	&code_090Code,
	&code_091Code,
	&code_092Code,
	&code_093Code,
	&code_094Code,
	&code_095Code,
	&code_096Code,
	&code_097Code,
	&code_098Code,
	&code_099Code,
	&code_100Code,
	&code_101Code,
	&code_102Code,
	&code_103Code,
	&code_104Code,
	&code_105Code,
	&code_106Code,
	&code_107Code,
	&code_108Code,
	&code_109Code,
	&code_110Code,
	&code_111Code,
	&code_112Code,
	&code_113Code,
	&code_114Code,
	&code_115Code,
	&code_116Code,
	&code_117Code,
	&code_118Code,
	&code_119Code,
	&code_120Code,
	&code_121Code,
	&code_122Code,
	&code_123Code,
	&code_124Code,
	&code_125Code,
	&code_126Code,
	&code_127Code,
	&code_128Code,
	&code_129Code,
	&code_130Code,
	&code_131Code,
	&code_132Code,
	&code_133Code,
	&code_134Code,
	&code_135Code,
	&code_136Code,
	&code_137Code,
	&code_138Code,
	&code_139Code,
};
uint8_t num_codes = NUM_ELEM(powerCodes);


#endif

////////////////////////////////////////////////////////////////


