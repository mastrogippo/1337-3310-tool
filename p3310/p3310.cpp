/*
  Library to handle various phone functions.
  Includes a lightweight font and bitmap library and
  LCD drivers based on Adafruit_PCD8544
 
    Copyright (C) 2015 Cristiano Griletti

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/ 

#include <p3310.h>
#include <SPI.h>

//#include <stdio.h>
//#include <string.h>
#include <inttypes.h>
#include "Arduino.h"
#include <avr/pgmspace.h>

//P3310::P3310(){}


void P3310::IOinit()
{
	pinMode(OP_CS, OUTPUT);
	pinMode(LCD_CS, OUTPUT);
	pinMode(LCD_DC, OUTPUT);
	pinMode(LCD_RES, OUTPUT);
	pinMode(EE_CS, OUTPUT);
	pinMode(buzz, OUTPUT);
	pinMode(LEDp, OUTPUT);
	pinMode(btnPWR, INPUT);
	pinMode(PWMaux, OUTPUT);
	
	//Analog
	pinMode(OPin, INPUT);
	pinMode(btnA, INPUT);
	pinMode(Vbat, INPUT);
	
	digitalWrite(OP_CS,HIGH);
	digitalWrite(LCD_CS,HIGH);
	digitalWrite(EE_CS,HIGH);
	digitalWrite(buzz,LOW);
	digitalWrite(LEDp,LOW);
	digitalWrite(btnPWR,HIGH); //Pullup
	digitalWrite(PWMaux,LOW);
	
	analogReference(EXTERNAL);
	//tone(buzz, 1000, 50);

	InitChars();
}

///Battery
uint16_t P3310::GetBatt()
{
	uint16_t tmp = analogRead(Vbat) * 55;
	tmp /= 10;
	return tmp;
}

///Keyboard
uint8_t P3310::GetBtn()
{
	uint16_t tmp = analogRead(btnA);
	
	if(tmp > 1020) return 0;
	
	if((tmp > BCmin) && (tmp < BCmax)) return BCm;
	if((tmp > BMmin) && (tmp < BMmax)) return BMm;
	if((tmp > BUmin) && (tmp < BUmax)) return BUm;
	if((tmp > BDmin) && (tmp < BDmax)) return BDm;
		
	return 0;
	
	
}

///BACKLIGHT
void P3310::setBacklight(uint8_t val)
{
	//if(val > 100)	//Let's not overdrive LEDs
	//	val = 100;
	analogWrite(LEDp, val);
}

/// LCD STUFF
void P3310::LCDinit(uint8_t contrast, uint8_t bias)
{
	SPI.begin();
	SPI.setClockDivider(PCD8544_SPI_CLOCK_DIV);
	SPI.setDataMode(SPI_MODE0);
	SPI.setBitOrder(MSBFIRST);
	digitalWrite(LCD_RES, LOW);
	_delay_ms(500);
	digitalWrite(LCD_RES, HIGH);
	
	command(PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION );// get into the EXTENDED mode!
	command(PCD8544_SETBIAS | bias);							// LCD bias select (4 is optimal?)
	
	if (contrast > 0x7f)										// set VOP
	contrast = 0x7f;
	command( PCD8544_SETVOP | contrast);						// Experimentally determined
	command(PCD8544_FUNCTIONSET);								// normal mode
	command(PCD8544_DISPLAYCONTROL | PCD8544_DISPLAYNORMAL);	// Set display to Normal

	// Push out pcd8544_buffer to the Display (will show the AFI logo)
	display();
}

void P3310::command(uint8_t c) {
	digitalWrite(LCD_DC, LOW);
	digitalWrite(LCD_CS, LOW);
	SPI.transfer(c);
	digitalWrite(LCD_CS, HIGH);
}

void P3310::data(uint8_t c) {
	digitalWrite(LCD_DC, HIGH);
	digitalWrite(LCD_CS, LOW);
	SPI.transfer(c);
	digitalWrite(LCD_CS, HIGH);
}

void P3310::setContrast(uint8_t val) {
	if (val > 0x7f) {
		val = 0x7f;
	}
	command(PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION );
	command( PCD8544_SETVOP | val);
	command(PCD8544_FUNCTIONSET);
	
}

void P3310::display(void) {
	uint8_t col, p;
	uint16_t baddr;
	baddr = 0;
	for(p = 0; p < 6; p++) {

		command(PCD8544_SETYADDR | p);
		command(PCD8544_SETXADDR);
		
		digitalWrite(LCD_DC, HIGH);
		digitalWrite(LCD_CS, LOW);
		for(col = 0; col < LCDWIDTH; col++) {
			SPI.transfer(lcd_buffer[baddr++]);
		}
		digitalWrite(LCD_CS, HIGH);

	}
	command(PCD8544_SETYADDR );  // no idea why this is necessary but it is to finish the last byte?
}

void P3310::clearDisplay(void) {
	memset(lcd_buffer, 0, LCDWIDTH*LCDHEIGHT/8);
}

void P3310::putBmp(uint16_t EEplace, uint8_t x, uint8_t y) {
	uint8_t w, h;
	uint16_t buffpos;
	
	EEplace += idx_zbmp;
	
	w = EEreadbyte(EEplace++);
	h = EEreadbyte(EEplace++);
	/*Serial.print(EEplace);
	Serial.print(" ");
	Serial.print(w);
	Serial.print(" ");
	Serial.print(h);
	Serial.print(" ");
	Serial.print(EEreadbyte(EEplace));
	Serial.print(" ");
	
	Serial.println(" ");*/
	
	if((w==LCDWIDTH)&&(h==LCDHEIGHT))
	{//full screen bitmap shortcut
		EEreadmem(EEplace, lcd_buffer, LCDWIDTH*LCDHEIGHT/8);
		return;
	}
	
	buffpos = (x*LCDWIDTH) + y;
	//x is the row, not the pixel!
	for(int i = 0; i <= (h /8); i++)
	{
		EEreadmem(EEplace, lcd_buffer+buffpos, w);
		EEplace+=w;
		buffpos += LCDWIDTH;
	} 
}

void P3310::battBar(void)
{
	uint16_t pos = (5*LCDWIDTH)-4;
	lcd_buffer[pos++] = 0x1F;
	lcd_buffer[pos++] = 0x11;
	lcd_buffer[pos++] = 0x11;
	lcd_buffer[pos++] = 0x1F;
	uint8_t bat = (byte)((GetBatt()) >> 5);
	
	pos = (3*LCDWIDTH);
	lcd_buffer[pos++] = 0x80;
	lcd_buffer[pos++] = 0x80;
	lcd_buffer[pos++] = 0x80;
	lcd_buffer[pos++] = 0x80;
	lcd_buffer[pos++] = 0x80;
	
	pos = (4*LCDWIDTH);
	lcd_buffer[pos++] = 0x01;
	lcd_buffer[pos++] = 0x02;
	lcd_buffer[pos++] = 0x1F;
	lcd_buffer[pos++] = 0x02;
	lcd_buffer[pos++] = 0x01;
	
	if(bat >= 108) //1 (3.45v)
	{
		pos = (4*LCDWIDTH)-3;
		lcd_buffer[pos++] = 0x80;
		lcd_buffer[pos++] = 0xBF;
		lcd_buffer[pos++] = 0x3F;
	}
	else
	{
		pos = (4*LCDWIDTH)-3;
		lcd_buffer[pos++] = 0x80;
		lcd_buffer[pos++] = 0x80;
		lcd_buffer[pos++] = 0;
	}
	
	
	if(bat >= 111) //2 (3.55v)
	{
		pos = (3*LCDWIDTH)-2;
		lcd_buffer[pos++] = 0x7F;
		lcd_buffer[pos++] = 0x7F;
	}

	if(bat >= 116) //3 (3.7v)
	{
		pos = (2*LCDWIDTH)-3;
		lcd_buffer[pos++] = 0x7F;
		lcd_buffer[pos++] = 0x7F;
		lcd_buffer[pos++] = 0x7F;
	}
	
	if(bat >= 122) //4 (3.9v)
	{
		pos = (1*LCDWIDTH)-4;
		lcd_buffer[pos++] = 0x7F;
		lcd_buffer[pos++] = 0x7F;
		lcd_buffer[pos++] = 0x7F;
		lcd_buffer[pos++] = 0x7F;
	}
}

///FONTS
struct Font{
	uint16_t EEMainOff;
	uint8_t *BigOffs; //array with block offsets from the main one
	const prog_char PROGMEM  *chars;
	//const uint8_t (*chars)[3][0x20]; //array of arrays of offsets for single characters
};
Font fonts[2];

void P3310::InitChars()
{
	//Small/bold
	fonts[0].EEMainOff = idx_font_sb;
	fonts[0].BigOffs = OffsetsSB;
	fonts[0].chars = &fntSB[0];
	//Small/plain
	fonts[1].EEMainOff = idx_font_sp;
	fonts[1].BigOffs = OffsetsSP;
	fonts[1].chars = &fntSP[0];
}

void P3310::LCDputsL(char* str, uint8_t line, uint8_t col)
{
	uint8_t i, wid;
	uint8_t cha;
	uint16_t eeaddr, buffadd;
	
	buffadd = (line * 84) + col;
	while(str[0] != 0)
	{
//Serial.print(str[0]);
		if(str[0] == ' ')
		{
			buffadd += 3;
			str++;
			continue;
		}
		if(str[0] == '.')
		{
			lcd_buffer[buffadd+84] = 0x0C;
			lcd_buffer[buffadd+85] = 0x0C;
			buffadd += 3;
			str++;
			continue;
		}
		
		if((str[0] < 0x30) || (str[0] > 0x7A)) str[0] = '?';

/*Serial.print(" c:");
Serial.print(str[0] - 0x30);
Serial.print(" readw:");
Serial.print(pgm_read_word_near(str[0] - 0x30));*/
		eeaddr = idx_font_l + pgm_read_word_near(fnt1 + (str[0] - 0x30));//fnt1[str[0] - 0x30];
		wid = EEreadbyte(eeaddr++);
//Serial.print(" wid:");
//Serial.print(wid);
		//Should I add something to jump to the next line instead of cutting the character?
		//Nah; I wouldn't want it to wrap at the end of the screen anyway.
		for(i = 0; i < wid; i++)
		{
			lcd_buffer[buffadd+84] = EEreadbyte(eeaddr++);
			lcd_buffer[buffadd++] = EEreadbyte(eeaddr++);
		}
		buffadd++; //white space after char
		str++;
	}
	//display();
}

void P3310::LCDputs(char* str, uint8_t line, uint8_t col, uint8_t nfont)
{
	uint8_t i, p, wid;
	uint8_t cha;
	uint16_t eeaddr, buffadd;
	
	buffadd = (line * 84) + col;
	while(str[0] != 0)
	{
		p = 0;
		cha = str[0] - 0x20;
		//Serial.print(str[0]);
		//Serial.print(" cha:");
		//Serial.print(cha, HEX);
		
		if(cha >= 0x60) cha = 0x59; //0x59 = unknown char
		
		
		eeaddr = fonts[nfont].EEMainOff;
		while(cha >= 0x20)
		{
			eeaddr += fonts[nfont].BigOffs[p];
			cha -= 0x20;
			p++;
		}
		eeaddr += pgm_read_byte_near(fonts[nfont].chars + cha + (p * 0x20));
		//eeaddr += fonts[nfont].chars[0][p][cha];
		//Serial.print(" idx:");
		//Serial.print(cha,HEX);
		//Serial.print(" eea:");
		//Serial.print(eeaddr);
		
		wid = EEreadbyte(eeaddr++);
//		Serial.print(" wid:");
//		Serial.print(wid);

		//Should I add something to jump to the next line instead of cutting the character?
		//Nah; I wouldn't want it to wrap at the end of the screen anyway.
		for(i = 0; i < wid; i++)
		{
			lcd_buffer[buffadd++] = EEreadbyte(eeaddr++);
//			Serial.print(" ");
//			Serial.print(lcd_buffer[buffadd-1],HEX);
		}
//		Serial.println(" ");
		buffadd++; //white space after char
		str++;
	}
	//display();
}

void P3310::SetPx(uint8_t xp, uint8_t yp)
{
	uint8_t tmp = lcd_buffer[(LCDWIDTH * (yp/8)) + xp];
	lcd_buffer[(LCDWIDTH * (yp/8)) + xp] |= (1 << (yp % 8));
}
/*

uint8_t 3310::GetPROGMEMbyte(const prog_char * pgm, uint8_t pos)
{
	char * a;

	// allocate the buffer to the same size as the progmem string
	a = (char *)realloc(a, strlen_P(pgm)+1);
	// copy from progmem
	strcpy_P(a, pgm);
	// return the byte at position
	return a[pos];
}*/