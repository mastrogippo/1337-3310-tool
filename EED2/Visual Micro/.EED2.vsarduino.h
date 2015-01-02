/* 
	Editor: http://www.visualmicro.com
	        visual micro and the arduino ide ignore this code during compilation. this code is automatically maintained by visualmicro, manual changes to this file will be overwritten
	        the contents of the Visual Micro sketch sub folder can be deleted prior to publishing a project
	        all non-arduino files created by visual micro and all visual studio project or solution files can be freely deleted and are not required to compile a sketch (do not delete your own code!).
	        note: debugger breakpoints are stored in '.sln' or '.asln' files, knowledge of last uploaded breakpoints is stored in the upload.vmps.xml file. Both files are required to continue a previous debug session without needing to compile and upload again
	
	Hardware: Pro Trinket 5V/16MHz (FTDI), Platform=avr, Package=arduino
*/

#define __AVR_ATmega328p__
#define __AVR_ATmega328P__
#define ARDUINO 158
#define ARDUINO_MAIN
#define F_CPU 16000000L
#define __AVR__
extern "C" void __cxa_pure_virtual() {;}

byte readB(long addr);
void readM(long addr, byte * buff, long size);
void writeM(long addr, byte * buff, int size);
int freeRam ();
//
//
void Smenu(uint8_t po);
void MenuInit(void);
void PrintVolt(void);
void PrintAmp(void);
void Graph(long *val, uint8_t row, uint16_t scale);
void Multimeter(void);
void PlayTune(uint8_t Stop);
void Test(void);
void BootAni(void);
void serialEvent();

#include "E:\Elettronica\Arduino\arduino-1.5.8\hardware\arduino\avr\variants\eightanaloginputs\pins_arduino.h" 
#include "E:\Elettronica\Arduino\arduino-1.5.8\hardware\arduino\avr\cores\arduino\arduino.h"
#include "E:\Elettronica\3310BT\Ardu\EED2\EED2\EED2.ino"
#include "E:\Elettronica\3310BT\Ardu\EED2\EED2\PGA.cpp"
#include "E:\Elettronica\3310BT\Ardu\EED2\EED2\PGA.h"
#include "E:\Elettronica\3310BT\Ardu\EED2\EED2\TVB.cpp"
#include "E:\Elettronica\3310BT\Ardu\EED2\EED2\TVB.h"
#include "E:\Elettronica\3310BT\Ardu\EED2\EED2\menu.h"
#include "E:\Elettronica\3310BT\Ardu\EED2\EED2\tetris.cpp"
