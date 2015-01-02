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


#include <avr/pgmspace.h>
#define prog_char char PROGMEM

#include <spieeprom.h>
#include <bmp.h>
#include <PCD.h>

#define OP_CS 9
#define LCD_DC A2
#define LCD_RES A3
#define LCD_CS A4
#define EE_CS 10
#define btnA A1
#define OPin A0
#define buzz 6
#define LEDp 5
#define btnPWR 4
#define PWMaux 3
#define Vbat A5

#define BCmin 450 //Clear
#define BCmax 550
#define BMmin 250 //Menu
#define BMmax 350
#define BUmin 600 //UP
#define BUmax 700
#define BDmin 800 //DOWN
#define BDmax 900

//Mask for the button byte
#define BCm 0x01
#define BMm 0x02
#define BUm 0x04
#define BDm 0x08

#define DefBRT 255

#define EEsize 0xFFFF
#define idx_font_l 0
#define idx_font_sb 957
#define idx_font_sp 1443
#define idx_zbmp 1855
#define PGACalData EEsize - 8


#ifndef P3310_H_
#define P3310_H_

//Large font
const uint16_t PROGMEM fnt1[74] = {0, 15, 30, 45, 60, 75, 90, 105, 120, 135, 150, 157, 164, 175, 186, 197, 210, 229, 244, 259, 272, 287, 302, 317, 332, 347, 352, 363, 380, 393, 412, 427, 444, 459, 476, 493, 506, 519, 534, 551, 572, 587, 604, 0, 619, 0, 0, 628, 0, 645, 658, 671, 682, 695, 708, 717, 730, 743, 748, 755, 768, 773, 790, 803, 816, 829, 842, 853, 864, 873, 886, 899, 918, 931, };
		
//Small/bold font
const prog_char PROGMEM  fntSB[0x60] = {0, 3, 6, 10, 16, 22, 29, 36, 38, 42, 46, 52, 58, 61, 66, 69, 73, 79, 85, 91, 97, 103, 109, 115, 121, 127, 133, 136, 139, 144, 149, 154,
0, 7, 13, 19, 25, 31, 37, 43, 49, 55, 58, 63, 70, 75, 83, 90, 97, 103, 110, 116, 121, 128, 134, 141, 149, 156, 163, 0, 169, 0, 0, 173,
0, 0, 6, 12, 17, 23, 29, 33, 39, 45, 48, 52, 58, 61, 70, 76, 82, 88, 94, 99, 104, 108, 114, 120, 128, 134, 140, 0, 0, 0, 0, 0};

//Small/plain font
const prog_char PROGMEM fntSP[0x60] = {0, 3, 5, 9, 15, 20, 25, 31, 33, 36, 39, 45, 51, 54, 59, 61, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 118, 121, 126, 131, 136, 
0, 6, 11, 16, 21, 26, 31, 36, 41, 46, 48, 52, 57, 62, 68, 74, 80, 85, 91, 96, 101, 107, 112, 118, 126, 132, 138, 0, 143, 0, 0, 147, 
0, 0, 5, 10, 14, 19, 24, 27, 32, 37, 39, 42, 47, 49, 55, 60, 65, 70, 75, 79, 83, 86, 91, 97, 103, 108, 113, 0, 0, 0, 0, 0};
		

class P3310
{
	private:
		//EEPROM indexes
/*uint16_t idx_font_l = 0;
uint16_t idx_font_sb = 957;
uint16_t idx_font_sp = 1443;
uint16_t idx_zbmp = 1855;*/
		uint8_t OffsetsSB[2] = {160, 180};
		uint8_t OffsetsSP[2] = {141, 153}; //Group Offsets

		
		void InitChars();
		
		void command(uint8_t c);
		void data(uint8_t c);

		//uint8_t GetPROGMEMbyte(const prog_char * pgm, uint8_t pos);
		
	public:
		void IOinit();
	
		uint8_t GetBtn();
		uint16_t GetBatt();
	
		void setBacklight(uint8_t val = DefBRT);
	
		//LCD
		void LCDinit(uint8_t contrast = 40, uint8_t bias = 0x04);
		void setContrast(uint8_t val);
		void display(void);
		void clearDisplay(void);

		uint8_t lcd_buffer[LCDWIDTH * LCDHEIGHT / 8];
	
		byte (*EEreadbyte)(long);
		void (*EEreadmem)(long, byte *, long);
	
		void LCDputs(char* str, uint8_t line, uint8_t col, uint8_t nfont);
		void LCDputsL(char* str, uint8_t line, uint8_t col);
		
		void putBmp(uint16_t EEplace, uint8_t x, uint8_t y);
		
		void SetPx(uint8_t xp, uint8_t yp);
		
		void battBar(void);
	//SPIEEPROM(); // default to type 0
	//SPIEEPROM(byte type); // type=0: 16-bits address
	// type=1: 24-bits address
	// type>1: defaults to type 0
	
};


#endif