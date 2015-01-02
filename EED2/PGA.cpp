/*
    1337 3310 tool - a multitool in the form factor of the best phone ever
	This file handles communication with the PGA and calculates stuff
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


#include "PGA.h"
#include <p3310.h>
#include <SPI.h>

void PGA::init()
{
  	pinMode(OP_CS, OUTPUT);
  	//SPI.begin(); //I assume I already did it earlier
	//SPI.setDataMode(SPI_MODE0);
	SetPGA(0,0);
	/*EEreadmem(PGACalData, gains, 8);
	
	if(gains[0] == 0) //init calibration
	{
		gain[0] = 1;
		gain[1] = 2;
		gain[2] = 5;
		gain[3] = 10;
		gain[4] = 20;
		gain[5] = 50;
		gain[6] = 100;
		gain[7] = 200;
		EEwritemem(PGACalData, gains, 8);
	}*/
	analogReference(EXTERNAL); 
}


//const Vref = 2048;
//resolution: 1024
const int bitVal = 2; //mV
//uint8_t *gains; //load from EEprom for calibration
const uint8_t gains[8] = {1,2,5,10,20,50,100,200};
//const int gainswitch[8] = {512,204,102,512,204,102,512,0};//When to switch up gain
const int gainswitch[8] = {480,170,70,480,170,70,480,0};//When to switch up gain, with some room to play
uint8_t channel = 0;
uint8_t gain0 = 0;
uint8_t gain1 = 0;
long outval;
const int center = 512;

//boolean neg = false;
/*
void loop()
{

	MeasureCurent();
	delay(400);

	//Manual gain set
	if(Serial.available() > 0)
	{
	  Serial.read();
	  Serial.print("Gain: ");
	  gain++;
	  if(gain > 7) gain = 0;
	  Serial.println(gains[gain]);
	  SetPGA(gain,0);
	}
	
}*/
long PGA::MeasureCurrent(void)
{
	SetPGA(gain1, 1); //Switch to amp channel
	
	delay(25);
	int val = analogRead(A0);
	//val -= center;
	
	//if(abs(val) > 505) //Value too high, decrease gain
	if(val > 1021) //Value too high, decrease gain
	{
		if(gain1 > 0)
		{
			SetPGA(--gain1, 1);
			return -1;
			//Serial.print("New gain: ");
			//Serial.println(gains[gain]);
		}
		else
		{//Overflow
			return -1;
		}
	}
	//else if(abs(val) < gainswitch[gain]) //I have room to measure this with a higher gain, better accuracy
	else if(val < gainswitch[gain1]) //I have room to measure this with a higher gain, better accuracy
	{//Increase gain
		if(gain1 < 7) //Shouldn't need this
		{
			SetPGA(++gain1, 1);
			return -1;
			//Serial.print("New gain: ");
			//Serial.println(gains[gain]);
		}
	}
	else
	{
		//Serial.print(val);
		//Serial.print(" -- ");
		
		//outval = val;
		outval = ((val-1) << 1); //Val at AD input
		//Serial.print(outval);
		//Serial.print(" -- ");
		
		outval *= Vdiv;
		outval /= gains[gain1];
		outval /= R3;
		//Serial.println(outval); //Original value
		return outval/10; //milliamp
		//delay(450);
	}
}

long PGA::MeasureVoltage(long Current) 
{
	SetPGA(gain0, 0); //Switch to voltage channel
	
	delay(25);
	int val = analogRead(OPin);
	//val -= center;	
		
	//if(abs(val) > 505) //Value too high, decrease gain
	if(val > 1021) //Value too high, decrease gain
	{
		if(gain0 > 0)
		{
			SetPGA(--gain0, 0);
			return -1;
			//Serial.print("New gain: ");
			//Serial.println(gains[gain]);
		}
		else
		{//Overflow
			return -1;
		}
	}
	//else if(abs(val) < gainswitch[gain]) //I have room to measure this with a higher gain, better accuracy
	else if(val < gainswitch[gain0]) //I have room to measure this with a higher gain, better accuracy
	{//Increase gain
		if(gain0 < 7) //Shouldn't need this
		{
			SetPGA(++gain0, 0);
			return -1;
			//Serial.print("New gain: ");
			//Serial.println(gains[gain]);
		}
	}
	else
	{
		//Serial.print(val);
		//Serial.print(" -- ");
			
		//outval = val;
		outval = ((val-1) << 1); //Val at AD input
		//Serial.print(outval);
		//Serial.print(" -- ");
		
		//Serial.print(outval);
		//Serial.print(" -- ");
		outval *= 100; //let's not lose too much precision, without having to use floats
		
		outval /= gains[gain0]; // before pga
		
		//If I'm measuring current at the same time, I have to subtract it from the raw voltage....
		outval -= Current*100; //-offset
		
		outval *= Vdiv; //divider
		/*outval *= Vdiv;
		outval /= gains[gain0];
		
		outval -= Current*gains[gain0];*/
		
		//Serial.println(outval); //Original value
		return outval/100;
		//delay(450);
	}
}

double PGA::MeasureRes(uint8_t lowMode)
{
	if(channel != 0)
	{
		if((lowMode != 0) && (gain0 != 0))
			SetPGA(gain0, 0); //Switch to voltage channel
		
		if(lowMode == 0)
			SetPGA(gain0, 0); //Switch to voltage channel
	}
	//if(lowMode) gain0 = 0;
	//if((channel != 0) || (gain0 != 0)) && (lowMode != 0
	//	SetPGA(gain0, 0); //Switch to voltage channel
	
	//delay(25);
	int val = analogRead(OPin);
	//val -= center;
	
	//Serial.print("val ");
	//Serial.println(val); //Original value
	if(lowMode != 0)
	{
		//Serial.print("volt");
		//Serial.println(val); //Original value
		if(val > 1020) //short!
			return 1;
		return 1/0;
	}//if(abs(val) > 505) //Value too high, decrease gain
	else if(val > 1021) //Value too high, decrease gain
	{
		if(gain0 > 0)
		{
			SetPGA(--gain0, 0);
			//Serial.print("New gain: ");
			//Serial.println(gains[gain]);
		}
		else
		{//Overflow
			return -1;
		}
	}
	//else if(abs(val) < gainswitch[gain]) //I have room to measure this with a higher gain, better accuracy
	else if(val < gainswitch[gain0]) //I have room to measure this with a higher gain, better accuracy
	{//Increase gain
		if(gain < 7) //Shouldn't need this
		{
			SetPGA(++gain0, 0);
			//Serial.print("New gain: ");
			//Serial.println(gains[gain]);
		}
	}
	else
	{
		//Serial.print(val);
		//Serial.print(" -- ");
		
		//outval = val;
		outval = ((val-1) << 1); //Val at AD input
		//Serial.print(outval);
		//Serial.print(" -- ");
		
		//outval *= Vdiv;
		outval /= gains[gain0]; //val at opamp input
		//Serial.print("volt");
		//Serial.println(outval); //Original value
		
		/*double I = (double)(outval)/(double)(R2);
		I = ((double)(vref) - (double)(outval)) / I;*/
		//double I = (double)(((vref-outval)/1000)*(R2/10)) / (double)(outval);
		double I = (double)((vref-outval)*(R2/10)) / (double)(outval);
		//Serial.print("ohm");
		//Serial.print(I);
		
		//Serial.println(outval); //Original value
		return I+50;
		//delay(450);
	}
}

void PGA::SetPGA(uint8_t G, uint8_t Ch) {
	channel = Ch;
	// take the SS pin low to select the chip:
	digitalWrite(OP_CS, LOW);
	//  send in the address and value via SPI:
	SPI.transfer(0x2A);
	SPI.transfer((G<<4) + Ch);
	// take the SS pin high to de-select the chip:
	digitalWrite(OP_CS, HIGH);
}
