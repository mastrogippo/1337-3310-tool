/*
    1337 3310 tool - a multitool in the form factor of the best phone ever
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

#include <SPI.h>
#include <spieeprom.h>
#include <p3310.h>
#include "PGA.h"
#include "TVB.h"
#include "menu.h"
#include <string.h>

P3310 phone;
PGA pga1;
#define BootAniEnabled //to save time during tests

//This put the phone in EEPROM writing mode
//Everything written on the serial will be copied to the memory
//#define EEWRITE

SPIEEPROM disk1(1); // parameter is type
// type=0: 16-bits address
// type=1: 24-bits address
// type>1: defaults to type 0
/*
#ifdef test
	#include <Adafruit_GFX.h>
	#include <Adafruit_PCD8544.h>
	Adafruit_PCD8544 display = Adafruit_PCD8544(LCD_DC, LCD_CS, LCD_RES);
	//Adafruit_PCD8544 display = Adafruit_PCD8544(A2, A4, A3);
#endif*/
/*
void setup() {
	Serial.begin(115200);
	phone.IOinit();
	
	//IOinit();
	tone(buzz, 500, 10);
	
	disk1.setup(); // setup disk1

	disk1.start_write();
	disk1.send_address(0); // send address

}

void loop() {

	
}

void serialEvent() {
	while (Serial.available()) {
		SPI.transfer(Serial.read()); // Fetch the received byte value into the variable "ByteReceived"
	}
}*/

extern void setupT();
extern void Smenu(uint8_t po = 0);

unsigned long time;
unsigned long delBtn;
char tmpS[10]; //temp string for printfs
uint8_t tmpBtn;

byte readB(long addr)
{
	return disk1.read_byte(addr);
}
void readM(long addr, byte * buff, long size)
{
	disk1.read_mem(addr, buff, size);
}
void writeM(long addr, byte * buff, int size)
{
	disk1.write(addr, buff, size);
}

int freeRam () {
	extern int __heap_start, *__brkval;
	int v;
	return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void setup() {
	Serial.begin(57600);
	phone.IOinit();
	phone.LCDinit(64,4);
	phone.setBacklight(0);
	tone(buzz, 1000, 50);
	
	
	phone.EEreadbyte = &readB;
	phone.EEreadmem = &readM;
	
	MenuInit();
	disk1.setup(); // setup disk1
	
	Serial.println("hello");
	pga1.EEreadbyte = &readB;
	pga1.EEreadmem = &readM;
	pga1.EEwritemem = &writeM;
	pga1.init();
	
	
	//setupTVB();
	
	Serial.println("init");
	
	Serial.println(freeRam());
	
#ifdef EEWRITE
	delay(3000);
	disk1.start_write();
	disk1.send_address(0); // send address
	
	//empty buffer
	while (Serial.available()) {
		Serial.read(); // Fetch the received byte value into the variable "ByteReceived"
	}
	tone(buzz, 1000, 200);
#endif


}

uint8_t Power = 0;
uint8_t Screen = 0;
void loop() {
#ifdef EEWRITE
	if(phone.GetBtn())
	{
		digitalWrite(SLAVESELECT,HIGH);
		
		while (disk1.isWIP()) {
			delay(1);
		}
		phone.setBacklight(0);
		tone(buzz, 500, 20);
		while(1);
	}
	/*while(1)
	{
		
			Serial.print("batt: ");
			Serial.print(phone.GetBatt());
			Serial.print(" btn: ");
			Serial.println(phone.GetBtn(),HEX);
			//if(
		delay(100);
	}*/
#else
	
	Test();
//Serial.print(Screen);

	if(!digitalRead(btnPWR))
	{
		Serial.print('.');
		//Add sleep and peripheral shutdown...
		for(uint8_t i = 0; i < 50; i++) //1 second button
		{
			delay(20);
			if(digitalRead(btnPWR))
				return;
		}
		if(Power == 0)
		{
			phone.setBacklight();
			tone(buzz, 500, 20);
			BootAni();
			Power = 1;
		}
		else if(Power == 1)
		{
			phone.setBacklight(0);
			tone(buzz, 500, 20);
			phone.clearDisplay();
			phone.display();
			Power = 0;
		}
		
	}
	if(Power == 0) return;
	
	switch(Screen)
	{
		case 0: //main screen
			phone.clearDisplay();
			phone.battBar();
			phone.LCDputs("Menu", 5, 29, 0);
			phone.display();
			if(phone.GetBtn() == BMm) //Menu
			{
				delBtn = millis() + 500;
				Screen++;
			}
			delay(20);
		break;
		
		case 1: //main menu
			Smenu(0);
			break;
			
		case 50:
			Multimeter();
			break;
		
		case 51:
			sendAllCodes();
			delBtn = millis() + 500;
			Screen = 1;
			Smenu(1);
			break;
			
		case 53:
			setupT();
			delBtn = millis() + 500;
			Screen = 1;
			Smenu(1);
			break;
		default: Screen = 1;
	}

	/*while(1)
	{
		phone.clearDisplay();
		phone.battBar();
		phone.display();
		delay(200);
	};


	phone.clearDisplay();
	phone.LCDputs(">HELLO world", 0, 0, 0);
	phone.LCDputs(">HELLO world", 1, 0, 1);
	phone.LCDputsL(">HELLO world", 2, 0);
	phone.display();
	delay(200);

	phone.clearDisplay();
	phone.LCDputs("1234567890", 0, 0, 0);
	phone.LCDputsL("Phone book", 1, 5);
	phone.putBmp(bmp087, 3, 12);
	phone.LCDputs("Select", 5, 29, 0);
	phone.display();
	delay(3000);
	
	while(1);
	/*
disk1.read_mem(0, &phone.lcd_buffer[0],504);
phone.display();
while(1);

	/*Serial.println("display");
	for(long i = 0; i < 9; i++)
	{
		
		disk1.read_mem(i * 504, &phone.lcd_buffer[0],504);
		phone.display();
		delay(2000);
	}
	delay(2000);*/
#endif
}

void Smenu(uint8_t po)
{
	static uint8_t Pos = 0;
	static uint8_t CurMen = 0;
	static uint8_t tmp = 0;
	
	if(po == 1) Pos = 1;
	//Serial.print(Pos);
	switch(Pos)
	{
		case 0:
			CurMen = 0;
			Pos++;
		case 1: //draw screen
			sprintf(tmpS, "%d", CurMen+1);
			phone.clearDisplay();
			if(CurMen < 10) phone.LCDputs(tmpS, 0, LCDWIDTH-8, 0);
				else phone.LCDputs(tmpS, 0, LCDWIDTH-16, 0);
			phone.LCDputsL(ms[CurMen].Name, 1, ms[CurMen].nPad);
			phone.putBmp(ms[CurMen].Bmp, 3, 12);
			phone.LCDputs("Select", 5, 29, 0);
			phone.display();
			tmp = 1; //animation index
			delay(100);
			time = millis(); //prepare timer
			Pos++;	
		//break;
		
		case 2: //screen animation
			if(tmp < ms[CurMen].nAni)
			{
				if((millis() - time) > 200)
				{
					phone.putBmp(ms[CurMen].Bmp + (130*tmp++), 3, 12);
					phone.display();
					time = millis();
				}	
			}
			
			if(delBtn < millis())
			{
				tmpBtn = phone.GetBtn();
				switch(tmpBtn)
				{
					case BMm: Screen = 50 + CurMen; return;
					case BCm: Screen = 0; Pos = 0; return;
					case BDm:
						CurMen++;
						if(CurMen >= NumMenu) CurMen = 0;
						Pos = 1;
					break;
					case BUm:
						if(CurMen == 0) CurMen = NumMenu;
						Pos = 1;
						CurMen--;
					break;
				}
				if(tmpBtn!=0) delBtn = millis() + 400;
			}
			
		break;
		
	}
	
	//while(phone.GetBtn() != 0) delay(20); //wait button release	
}

void MenuInit(void)
{
	tmpBtn = 0;

	ms[tmpBtn].Bmp = bmp151;
	ms[tmpBtn].nAni = 13;
	ms[tmpBtn].Name = "Multimeter";
	ms[tmpBtn].nPad = 13;
	tmpBtn++;
	
	ms[tmpBtn].Bmp = bmp186;
	ms[tmpBtn].nAni = 8;
	ms[tmpBtn].Name = "TVBGone";
	ms[tmpBtn].nPad = 15;
	tmpBtn++;
	
	ms[tmpBtn].Bmp = bmp105;
	ms[tmpBtn].nAni = 12;
	ms[tmpBtn].Name = "IRC";
	ms[tmpBtn].nPad = 30;
	tmpBtn++;
	
	ms[tmpBtn].Bmp = bmp147;
	ms[tmpBtn].nAni = 4;
	ms[tmpBtn].Name = "Tetris";
	ms[tmpBtn].nPad = 22;
	tmpBtn++;
	
	ms[tmpBtn].Bmp = bmp128;
	ms[tmpBtn].nAni = 8;
	ms[tmpBtn].Name = "Settings";
	ms[tmpBtn].nPad = 18;
}

//MULTIMETER STUFF
long tmpLong;
long tmpVolt;
long tmpAmp;
long tmpWatt;
unsigned long tmpAvgWatt;
long tmpAvgCount;
double tmpOhm;

void PrintVolt(void){ //Put Voltage on tmpS
	
	tmpLong = pga1.MeasureVoltage(tmpAmp);
	//Serial.print(tmpVolt);
	if(tmpLong == -1) return;
	
	tmpVolt = tmpLong;
	/*sprintf(tmpS, "Overload");
	else */if(tmpLong < 3000)
	{//3 decimal hack
		tmpLong += 10000;
		sprintf(tmpS+1, "%d", tmpLong);
		sprintf(tmpS, "%d", tmpLong/1000);
		tmpS[0] = ' ';
		tmpS[2] = '.';
	}
	else
	{//2 decimal hack
		if(tmpLong < 10000)
		{
			tmpLong += 10000;
			tmpS[9] = 0x55;
		}
		else
		tmpS[9] = 0;
		sprintf(tmpS+1, "%d", tmpLong);
		sprintf(tmpS, "%d", tmpLong/1000);
		
		if(tmpS[9] != 0) tmpS[0] = ' ';
		tmpS[2] = '.';
		tmpS[5] = 0;
	}
}

void PrintAmp(void)
{
	tmpLong = pga1.MeasureCurrent();
	if(tmpLong == -1) return;
	tmpAmp = tmpLong;
	sprintf(tmpS, "%3d", tmpAmp);
	//Serial.println(tmpS);
}

uint8_t Gbuff[LCDWIDTH+1];
void Graph(long *val, uint8_t row, uint16_t scale)
{
	static uint8_t idx = 0;
	uint8_t tmp;
	if(val[0] == -1) return;
	Gbuff[idx++] = val[0] / (scale/16);
	if(idx >= LCDWIDTH) idx = 0;
	tmp = idx+1;
	for(int i = 0; i < LCDWIDTH; i++)
	{
		if(tmp >= LCDWIDTH)
			tmp = 0;
		
		if(Gbuff[tmp] > 8)
			phone.lcd_buffer[(row * LCDWIDTH) + i] |= (1 << (7-(Gbuff[tmp]-8)));
		else
			phone.lcd_buffer[((row+1) * LCDWIDTH) + i] |= (1 << (7-Gbuff[tmp]));
			
		tmp++;
	}
		
}

void Multimeter(void)
{
	long tmpl;
	static uint8_t Pos = 0;
	switch (Pos)
	{
		case 0: // V A W
			phone.clearDisplay();
			PrintAmp();
			phone.LCDputsL(tmpS, 2, 10);
			phone.LCDputsL("mA", 2, 62);
			PrintVolt();
			phone.LCDputsL(tmpS, 0, 10);
			phone.LCDputsL("V", 0, 62);
			
			tmpWatt = (tmpVolt * tmpAmp) /1000;
			sprintf(tmpS, "%3d", tmpWatt);
			phone.LCDputsL(tmpS, 4, 10);
			phone.LCDputsL("mW", 4, 62);

			phone.display();
			delay(50);
		break;
		case 1: // V A WG
			phone.clearDisplay();
			PrintAmp();
			phone.LCDputsL(tmpS, 2, 10);
			phone.LCDputsL("mA", 2, 62);
			PrintVolt();
			phone.LCDputsL(tmpS, 0, 10);
			phone.LCDputsL("V", 0, 62);
			
			tmpWatt = (tmpVolt * tmpAmp) /1000;
			sprintf(tmpS, "%3d", tmpWatt);
			phone.LCDputs(tmpS, 4, 0, 1);
			phone.LCDputs("mW", 5, 0, 1);

			Graph(&tmpWatt, 4, 500);
			
			phone.display();
			delay(50);
		break;
		case 2: // V W AG
			phone.clearDisplay();
			PrintAmp();
			phone.LCDputs(tmpS, 4, 0, 1);
			phone.LCDputs("mA", 5, 0, 1);
			PrintVolt();
			phone.LCDputsL(tmpS, 0, 10);
			phone.LCDputsL("V", 0, 62);
				
			tmpWatt = (tmpVolt * tmpAmp) /1000;
			sprintf(tmpS, "%3d", tmpWatt);
			phone.LCDputsL(tmpS, 2, 10);
			phone.LCDputsL("mW", 2, 62);

			Graph(&tmpAmp, 4, 250);
				
			phone.display();
			delay(50);
		break;
		case 3: // A W VG
			phone.clearDisplay();
			PrintAmp();
			phone.LCDputsL(tmpS, 0, 10);
			phone.LCDputsL("mA", 0, 62);
			PrintVolt();
			phone.LCDputs(tmpS, 4, 0, 1);
			phone.LCDputs("V", 5, 0, 1);
			
			tmpWatt = (tmpVolt * tmpAmp) /1000;
			sprintf(tmpS, "%3d", tmpWatt);
			phone.LCDputsL(tmpS, 2, 10);
			phone.LCDputsL("mW", 2, 62);

			Graph(&tmpVolt, 4, 7000);
			
			phone.display();
			delay(50);
		break;		
		case 4: // V A W avg
			phone.clearDisplay();
			PrintAmp();
			phone.LCDputs(tmpS, 0, 48, 0);
			phone.LCDputs("mA", 0, 68, 0);
			PrintVolt();
			phone.LCDputs(tmpS, 0, 5, 0);
			phone.LCDputs("V", 0, 35, 0);
			
			tmpWatt = (tmpVolt * tmpAmp) /1000;
			sprintf(tmpS, "%3d", tmpWatt);
			phone.LCDputs(tmpS, 1, 8, 0);
			phone.LCDputs("mW", 1, 40, 0);
			
			tmpAvgWatt += tmpWatt;//10;
			tmpAvgCount++;
			sprintf(tmpS, "%3d", tmpAvgWatt/tmpAvgCount);
			phone.LCDputs(tmpS, 2, 8, 0);
			phone.LCDputs("avg. mW", 2, 40, 0);
			Graph(&tmpWatt, 3, 500);
			
			phone.LCDputs("Reset", 5, 28, 0);
			phone.display();
			delay(50);
		break;
		case 5: //ohm
			phone.clearDisplay();
			tmpOhm = pga1.MeasureRes(0)+1;
			if(tmpOhm > 3000) 
			{
				tmpOhm /= 1000;
				phone.LCDputsL("Kohm", 0, 53);
			}
			else
			{
				phone.LCDputsL("ohm", 0, 60);
			}
			
			dtostrf(tmpOhm, 4,2,tmpS);//, "%f", tmpOhm);
			phone.LCDputsL(tmpS, 0, 2);
			phone.display();
			delay(50);
		break;
		case 6: //ohm beep
			tmpOhm = pga1.MeasureRes(1);
			if(tmpOhm == 1)
				PlayTune(0);
			else
				PlayTune(1);
		break;
	}
	
	tmpBtn = phone.GetBtn();
	switch(tmpBtn)
	{
		//case BMm: Screen = /*0; Pos = 0; return;//*/50 + CurMen; return;
		case BCm: 			
			delBtn = millis() + 400;
			Screen = 1;
			Smenu(1);
			return;
		case BDm: 
			memset(Gbuff, 0, LCDWIDTH +1);
			Pos++;
			if(Pos >= 7)
				Pos = 0;
			break;
		case BUm:
			memset(Gbuff, 0, LCDWIDTH +1);
			if(Pos == 0)
				Pos = 7;
			Pos--;
			break;				 
	}
	if((tmpBtn == BMm) && (Pos == 4))
	{
		//time = millis();
		tmpAvgCount = 0;
		tmpAvgWatt = 0;
	}
	
	if((tmpBtn == BUm) || (tmpBtn == BDm))
	{//First init pages
		if(Pos == 6) //Init Continuity page
		{
			phone.clearDisplay();
			phone.LCDputsL("Continuity", 0, 8);
			phone.display();		
		}
	}
	
	if(tmpBtn) delay(20);
	while(phone.GetBtn() != 0) delay(50); //wait button release
}

void PlayTune(uint8_t Stop)
{
	static uint8_t Pos = 50;
	static uint16_t nextdel = 0;
	
	if(Stop != 0)
	{
		Pos = 50;
		nextdel = 0;
		tone(buzz, 0, 2000);
		return;
	}
	if(((millis() - time) >= nextdel) || (nextdel == 0))
	{
		switch(Pos)
		{
			case 50:
				time = millis();
				Pos = 0;
			case 0: tone(buzz, 1397, 146); nextdel = 146; Pos++; break;
			case 1: tone(buzz, 1244, 146); nextdel = 146; Pos++; break;
			case 2: tone(buzz, 784 , 293); nextdel = 293; Pos++; break;
			case 3: tone(buzz, 880 , 293); nextdel = 293; Pos++; break;
			case 4: tone(buzz, 1175, 146); nextdel = 146; Pos++; break;
			case 5: tone(buzz, 1047, 146); nextdel = 146; Pos++; break;
			case 6: tone(buzz, 622 , 293); nextdel = 293; Pos++; break;
			case 7: tone(buzz, 698 , 293); nextdel = 293; Pos++; break;
			case 8: tone(buzz, 1047, 146); nextdel = 146; Pos++; break;
			case 9: tone(buzz, 932 , 146); nextdel = 146; Pos++; break;
			case 10: tone(buzz, 587 , 293); nextdel = 293; Pos++; break;
			case 11: tone(buzz, 698 , 293); nextdel = 293; Pos++; break;
			case 12: tone(buzz, 932 ,1171); nextdel = 1800; Pos = 50; break;
			//case 12: tone(buzz, 932 ,1171); nextdel = 1800; Pos = 50; break;
		}
		time = millis();
	}
}

void Test(void)
{
	/*for(int i = 0; i < 40; i++)
	{
		phone.clearDisplay();
		phone.SetPx(1,i);
		phone.display();
		delay(200);
	}*/
	//sendAllCodes();
/*	while(1){
		Serial.println("t");
		//SetPGA(1,1);
		pga1.MeasureRes(true);
		//Serial.println(pga1.MeasureVoltage());
		//pga1.MeasureVoltage();
		delay(100);
	}*/
}

void BootAni(void)
{
	#ifndef BootAniEnabled
		return;
	#endif
	phone.putBmp(bmpogo, 0, 0);
	phone.display();
	delay(2000);
	phone.putBmp(bmp330, 0, 0);
	phone.display();
	delay(250);
	phone.putBmp(bmp331, 0, 0);
	phone.display();
	delay(250);
	phone.putBmp(bmp332, 0, 0);
	phone.display();
	delay(250);
	phone.putBmp(bmp333, 0, 0);
	phone.display();
	delay(250);
	phone.putBmp(bmp334, 0, 0);
	phone.display();
	delay(250);
	phone.putBmp(bmp335, 0, 0);
	phone.display();
	delay(2000);
}


#ifdef EEWRITE
void serialEvent() {
	while (Serial.available()) {
		SPI.transfer(Serial.read()); // Fetch the received byte value into the variable "ByteReceived"
	}
}
#endif
