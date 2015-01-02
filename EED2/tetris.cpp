
// 
/* 
	
	Adapted from:	
		Code Example for jolliFactory 2X Bi-color LED Matrices Tetris Game example 1.1
		Tetris Code

		Adapted from Tetris.ino code by

			Jae Yeong Bae
			UBC ECE
			jocker.tistory.com

*/

#include <inttypes.h>
#include "Arduino.h"
#include <p3310.h>

/* ============================= Audio ============================= */
//int speakerOut = 9;

#define mC 1911
#define mC1 1804
#define mD 1703
#define mEb 1607
#define mE 1517
#define mF 1432
#define mF1 1352
#define mG 1276
#define mAb 1204
#define mA 1136
#define mBb 1073
#define mB 1012
#define mc 955
#define mc1 902
#define md 851
#define meb 803
#define me 758
#define mf 716
#define mf1 676
#define mg 638
#define mab 602
#define ma 568
#define mbb 536
#define mb 506

#define mp 0  //pause

/*
// MELODY and TIMING  =======================================
//  melody[] is an array of notes, accompanied by beats[],
//  which sets each note's relative length (higher #, longer note)
int melody[] = {mg};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int beats[]  = {8};

//int MAX_COUNT = sizeof(melody) / 2; // Melody length, for looping.
int MAX_COUNT = sizeof(melody) / sizeof(int); // Melody length, for looping.

// Set overall tempo
long tempo = 20000;
// Set length of pause between notes
int pause = 1000;
// Loop variable to increase Rest length
int rest_count = 100; //<-BLETCHEROUS HACK; See NOTES

// Initialize core variables
int tone_ = 0;
int beat = 0;
long duration  = 0;*/


extern P3310 phone;

/* ========================== Tetris Game ========================= */
long delays = 0;
short delay_ = 500;
long bdelay = 0;
short buttondelay = 150;
short btdowndelay = 30;
short btsidedelay = 80;
unsigned char blocktype;
unsigned char blockrotation;

boolean block[8][18]; //2 extra for rotation
boolean pile[8][16];
boolean disp[8][16];

boolean gameoverFlag = false;
//boolean selectColor = RED;

unsigned long startTime;
unsigned long elapsedTime;
int cnt = 0;

int buttonRotate = 4; // Rotate
int buttonRight = 5;  // Right
int buttonLeft = 6;   // Left
int buttonDown = 7;   // Down

boolean moveleft();
boolean moveright();
void updateLED();
void rotate();
void movedown();
boolean check_overlap();
void check_gameover();
void gameover();
void newBlock();
boolean space_below();
boolean space_left2();
boolean space_left3();
boolean space_left();
boolean space_right();
boolean space_right3();
boolean space_right2();
void LEDRefresh();
void loopT();

//**********************************************************************************************************************************************************  
void setupT() 
{
/*  pinMode(SPI_CS, OUTPUT);
  pinMode(speakerOut, OUTPUT); 
  
  TriggerSound();

  Serial.begin (9600);*/
  //Serial.println("jolliFactory Tetris LED Matrix Game example 1.1");              

  //SPI.begin(); //setup SPI Interface

  /*bi_maxTransferAll(0x0F, 0x00);   // 00 - Turn off Test mode
  bi_maxTransferAll(0x09, 0x00);   //Register 09 - BCD Decoding  // 0 = No decoding
  bi_maxTransferAll(0x0B, 0x07);   //Register B - Scan limit 1-7  // 7 = All LEDS
  bi_maxTransferAll(0x0C, 0x01);   // 01 = on 00 = Power saving mode or shutdown

  setBrightness();

  setISRtimer();                        // setup the timer
  startISR();                           // start the timer to toggle shutdown

  clearDisplay(GREEN);
  clearDisplay(RED);

  */
  int seed = 
  (analogRead(0)+1)*
  (analogRead(1)+1)*
  (analogRead(2)+1)*
  (analogRead(3)+1);
  randomSeed(seed);
  random(10,9610806);
  seed = seed *random(3336,15679912)+analogRead(random(4)) ;
  randomSeed(seed);  
  random(10,98046);

  /*
  cli();//stop interrupts

//set timer0 interrupt at 2kHz
  TCCR1A = 0;// set entire TCCR0A register to 0
  TCCR1B = 0;// same for TCCR0B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 2khz increments
  OCR1A = 259;// = (16*10^6) / (2000*64) - 1 (must be <256)
  // turn on CTC mode
  TCCR1A |= (1 << WGM01);
  // Set CS11 and CS10 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);   
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE0A);

  sei();//allow interrupts  
  

  pinMode(buttonRotate, INPUT_PULLUP); // Rotate
  pinMode(buttonRight, INPUT_PULLUP);  // Right
  pinMode(buttonLeft, INPUT_PULLUP);   // Left
  pinMode(buttonDown, INPUT_PULLUP);   // Down
    */
  
  newBlock();
  updateLED(); 
  
  while(digitalRead(btnPWR)) 
	loopT();  
}

long delBtnt;
//**********************************************************************************************************************************************************  
void loopT() 
{
  delay(30);
  
	if(!gameoverFlag)
	{		
		if (delays < millis())
		{
			delays = millis() + delay_;
			movedown();
		}
		
		if(delBtnt < millis())
		{
			int button = phone.GetBtn();
			switch(button)
			{
				case BMm: rotate(); break;
				case BCm: movedown(); break;
				case BDm: moveleft(); break;
				case BUm: moveright(); break;
			}
			if(button != 0) delBtnt = millis() + 150;
		}
		LEDRefresh();
	}
	else
	{
		if(delBtnt < millis())
		{
			int button = phone.GetBtn();
			switch(button)
			{
				case BMm: 
					gameoverFlag = false;
					
					for(int Ti=15;Ti>=0;Ti--)
					{
						for (int Tj=0;Tj<8;Tj++)
						{
							pile[Tj][Ti]=0;
						}
					} 
				break;
			}
		}
	}
}

//**********************************************************************************************************************************************************  
int Ti;
int Tj;
boolean moveleft()
{  
//  TriggerSound();
  
  if (space_left())
  {
    for (Ti=0;Ti<7;Ti++)
    {
      for (Tj=0;Tj<16;Tj++)      
      {
        block[Ti][Tj]=block[Ti+1][Tj];
      }
    }
    
    for (Tj=0;Tj<16;Tj++)      
    {
      block[7][Tj]=0;
    }    

    updateLED();
    return 1;
  }

  return 0;
}

//**********************************************************************************************************************************************************  
boolean moveright()
{
//  TriggerSound();
  
  if (space_right())
  {
    for (Ti=7;Ti>0;Ti--)
    {
      for (Tj=0;Tj<16;Tj++)      
      {
        block[Ti][Tj]=block[Ti-1][Tj];
      }
    }

    for (Tj=0;Tj<16;Tj++)      
    {
      block[0][Tj]=0;
    }    
    
   updateLED(); 
   return 1;   
  
  }
  return 0;
}

//**********************************************************************************************************************************************************  
void updateLED()
{ 
  for (Ti=0;Ti<8;Ti++)
  {
    for (Tj=0;Tj<16;Tj++)
    {
      disp[Ti][Tj] = block[Ti][Tj] | pile[Ti][Tj];
    }
  }
  LEDRefresh();
}

//**********************************************************************************************************************************************************  
void rotate()
{
//  TriggerSound();
  
  //skip for square block(3)
  if (blocktype == 3) return;
  
  int xi;
  int yi;
  //detect left
  for (Ti=7;Ti>=0;Ti--)
  {
    for (Tj=0;Tj<16;Tj++)
    {
      if (block[Ti][Tj])
      {
        xi = Ti;
      }
    }
  }
  
  //detect up
  for (Ti=15;Ti>=0;Ti--)
  {
    for (Tj=0;Tj<8;Tj++)
    {
      if (block[Tj][Ti])
      {
        yi = Ti;
      }
    }
  }  
    
  if (blocktype == 0)
  {
    if (blockrotation == 0) 
    {
      
      
      if (!space_left())
      {
        if (space_right3())
        {
          if (!moveright())
            return;
          xi++;
        }
        else return;
      }     
      else if (!space_right())
      {
        if (space_left3())
        {
          if (!moveleft())
            return;
          if (!moveleft())
            return;          
          xi--;
          xi--;        
        }
        else
          return;
      }
      else if (!space_right2())
      {
        if (space_left2())
        {
          if (!moveleft())
            return;          
          xi--;      
        }
        else
          return;
      }   
   
      
      block[xi][yi]=0;
      block[xi][yi+2]=0;
      block[xi][yi+3]=0;      
      
      block[xi-1][yi+1]=1;
      block[xi+1][yi+1]=1;
      block[xi+2][yi+1]=1;      

      blockrotation = 1;
    }
    else
    {
      block[xi][yi]=0;
      block[xi+2][yi]=0;
      block[xi+3][yi]=0;
      
      block[xi+1][yi-1]=1;
      block[xi+1][yi+1]=1;
      block[xi+1][yi+2]=1;

      blockrotation = 0;
    }    
  }
  
  //offset to mid
  xi ++;  
  yi ++;  
  
  if (blocktype == 1)
  {
    if (blockrotation == 0)
    {
      block[xi-1][yi-1] = 0;
      block[xi-1][yi] = 0;
      block[xi+1][yi] = 0;

      block[xi][yi-1] = 1;
      block[xi+1][yi-1] = 1;
      block[xi][yi+1] = 1;      
      
      blockrotation = 1;
    }
    else if (blockrotation == 1)
    {
      if (!space_left())
      {
        if (!moveright())
          return;
        xi++;
      }        
      xi--;
      
      block[xi][yi-1] = 0;
      block[xi+1][yi-1] = 0;
      block[xi][yi+1] = 0;      
      
      block[xi-1][yi] = 1;
      block[xi+1][yi] = 1;
      block[xi+1][yi+1] = 1;      
      
      blockrotation = 2;      
    }
    else if (blockrotation == 2)
    {
      yi --;
      
      block[xi-1][yi] = 0;
      block[xi+1][yi] = 0;
      block[xi+1][yi+1] = 0;      
      
      block[xi][yi-1] = 1;
      block[xi][yi+1] = 1;
      block[xi-1][yi+1] = 1;      
      
      blockrotation = 3;            
    }
    else
    {
      if (!space_right())
      {
        if (!moveleft())
          return;
        xi--;
      }
      block[xi][yi-1] = 0;
      block[xi][yi+1] = 0;
      block[xi-1][yi+1] = 0;        

      block[xi-1][yi-1] = 1;
      block[xi-1][yi] = 1;
      block[xi+1][yi] = 1;
      
      blockrotation = 0;          
    }  
  }



  if (blocktype == 2)
  {
    if (blockrotation == 0)
    {
      block[xi+1][yi-1] = 0;
      block[xi-1][yi] = 0;
      block[xi+1][yi] = 0;

      block[xi][yi-1] = 1;
      block[xi+1][yi+1] = 1;
      block[xi][yi+1] = 1;      
      
      blockrotation = 1;
    }
    else if (blockrotation == 1)
    {
      if (!space_left())
      {
        if (!moveright())
          return;
        xi++;
      }              
      xi--;
      
      block[xi][yi-1] = 0;
      block[xi+1][yi+1] = 0;
      block[xi][yi+1] = 0;      
      
      block[xi-1][yi] = 1;
      block[xi+1][yi] = 1;
      block[xi-1][yi+1] = 1;      
      
      blockrotation = 2;      
    }
    else if (blockrotation == 2)
    {
      yi --;
      
      block[xi-1][yi] = 0;
      block[xi+1][yi] = 0;
      block[xi-1][yi+1] = 0;      
      
      block[xi][yi-1] = 1;
      block[xi][yi+1] = 1;
      block[xi-1][yi-1] = 1;      
      
      blockrotation = 3;            
    }
    else
    {
      if (!space_right())
      {
        if (!moveleft())
          return;
        xi--;
      }      
      block[xi][yi-1] = 0;
      block[xi][yi+1] = 0;
      block[xi-1][yi-1] = 0;        

      block[xi+1][yi-1] = 1;
      block[xi-1][yi] = 1;
      block[xi+1][yi] = 1;
      
      blockrotation = 0;          
    }  
  }
  
  if (blocktype == 4)
  {
    if (blockrotation == 0)
    {
      block[xi+1][yi-1] = 0;
      block[xi-1][yi] = 0;

      block[xi+1][yi] = 1;
      block[xi+1][yi+1] = 1;      
      
      blockrotation = 1;
    }
    else
    {
      if (!space_left())
      {
        if (!moveright())
          return;
        xi++;
      }              
      xi--;
      
      block[xi+1][yi] = 0;
      block[xi+1][yi+1] = 0;      
      
      block[xi-1][yi] = 1;
      block[xi+1][yi-1] = 1;
      
      blockrotation = 0;          
    }  
  }  


  if (blocktype == 5)
  {
    if (blockrotation == 0)
    {
      block[xi][yi-1] = 0;
      block[xi-1][yi] = 0;
      block[xi+1][yi] = 0;

      block[xi][yi-1] = 1;
      block[xi+1][yi] = 1;
      block[xi][yi+1] = 1;      
      
      blockrotation = 1;
    }
    else if (blockrotation == 1)
    {
      if (!space_left())
      {
        if (!moveright())
          return;
        xi++;
      }              
      xi--;
      
      block[xi][yi-1] = 0;
      block[xi+1][yi] = 0;
      block[xi][yi+1] = 0;
      
      block[xi-1][yi] = 1;
      block[xi+1][yi] = 1;
      block[xi][yi+1] = 1;
      
      blockrotation = 2;      
    }
    else if (blockrotation == 2)
    {
      yi --;
      
      block[xi-1][yi] = 0;
      block[xi+1][yi] = 0;
      block[xi][yi+1] = 0;     
      
      block[xi][yi-1] = 1;
      block[xi-1][yi] = 1;
      block[xi][yi+1] = 1;      
      
      blockrotation = 3;            
    }
    else
    {
      if (!space_right())
      {
        if (!moveleft())
          return;
        xi--;
      }      
      block[xi][yi-1] = 0;
      block[xi-1][yi] = 0;
      block[xi][yi+1] = 0;      
      
      block[xi][yi-1] = 1;
      block[xi-1][yi] = 1;
      block[xi+1][yi] = 1;
      
      blockrotation = 0;          
    }  
  }
  
  if (blocktype == 6)
  {
    if (blockrotation == 0)
    {
      block[xi-1][yi-1] = 0;
      block[xi][yi-1] = 0;

      block[xi+1][yi-1] = 1;
      block[xi][yi+1] = 1;      
      
      blockrotation = 1;
    }
    else
    {
      if (!space_left())
      {
        if (!moveright())
          return;
        xi++;
      }              
      xi--;
      
      block[xi+1][yi-1] = 0;
      block[xi][yi+1] = 0;      
      
      block[xi-1][yi-1] = 1;
      block[xi][yi-1] = 1;
      
      blockrotation = 0;          
    }  
  }  

  //if rotating made block and pile overlap, push rows up
  while (!check_overlap())
  {
    for (Ti=0;Ti<18;Ti++)
    {
      for (Tj=0;Tj<8;Tj++)
      {
         block[Tj][Ti] = block[Tj][Ti+1];
      }
    }
    delays = millis() + delay_;
  }
  
  
  updateLED();    
}

//**********************************************************************************************************************************************************  
void movedown()
{ 
  if (space_below())
  {
    //move down
    for (Ti=15;Ti>=0;Ti--)
    {
      for (Tj=0;Tj<8;Tj++)
      {
        block[Tj][Ti] = block[Tj][Ti-1];
      }
    }
    for (Ti=0;Ti<7;Ti++)
    {
      block[Ti][0] = 0;
    }
  }
  else
  {
    //merge and new block    
    for (Ti=0;Ti<8;Ti++)
    {
     for(Tj=0;Tj<16;Tj++)
     {
       if (block[Ti][Tj])
       {
         pile[Ti][Tj]=1;
         block[Ti][Tj]=0;
       }
     }
    }
    newBlock();   
  }
  updateLED();  
}

//**********************************************************************************************************************************************************  
boolean check_overlap()
{ 
  for (Ti=0;Ti<16;Ti++)
  {
    for (Tj=0;Tj<7;Tj++)
    {
       if (block[Tj][Ti])
       {
         if (pile[Tj][Ti])
           return false;
       }        
    }
  }
  for (Ti=16;Ti<18;Ti++)
  {
    for (Tj=0;Tj<7;Tj++)
    {
       if (block[Tj][Ti])
       {
         return false;
       }        
    }
  }  
  return true;
}

//**********************************************************************************************************************************************************  
void check_gameover()
{
  int cnt=0;;
  int i,j,k;
  for(i=15;i>=0;i--)
  {
    cnt=0;
    for (j=0;j<8;j++)
    {
      if (pile[j][i])
      {
        cnt ++;
      }
    }    
    if (cnt == 8)
    {
      for (j=0;j<8;j++)
      {
        pile[j][i]=0;
      }        
      updateLED();
      delay(50);
      
      int k;
      for(k=i;k>0;k--)
      {
        for (j=0;j<8;j++)
        {
          pile[j][k] = pile[j][k-1];
        }                
      }
      for (j=0;j<8;j++)
      {
        pile[j][0] = 0;
      }        
      updateLED();      
      delay(50);      
      i++;    
    }
  }  
  
  for(i=0;i<8;i++)
  {
    if (pile[i][0])
      gameover();
  }
  return;
}

//**********************************************************************************************************************************************************  
void gameover()
{
  gameoverFlag = true;
  startTime = millis();       
  phone.LCDputs("Game Over", 2, 20, 0);
  phone.display();
  delay(1000);
  /*    
            
  while(true)      //To re-play if any buttons depressed again
  {      
    //int button = readBut();
    
    if ((button < 5) && (button > 0))
    {
      gameoverFlag = false;    
    
      for(Ti=15;Ti>=0;Ti--)
      {
        for (Tj=0;Tj<8;Tj++)
        {
          pile[Tj][Ti]=0;
        }             
      }
    
      break;
    }  
  }  */
}

//**********************************************************************************************************************************************************  
void newBlock()
{
  check_gameover();
  
/*  if (selectColor == RED)
    selectColor = GREEN;
  else
    selectColor = RED;*/

  
  blocktype = random(7);

  
  if (blocktype == 0)
  // 0
  // 0
  // 0
  // 0
  {
    block[3][0]=1;
    block[3][1]=1;
    block[3][2]=1;
    block[3][3]=1;      
  }

  if (blocktype == 1)
  // 0
  // 0 0 0
  {
    block[2][0]=1;
    block[2][1]=1;
    block[3][1]=1;
    block[4][1]=1;        
  }
  
  if (blocktype == 2)
  //     0
  // 0 0 0
  {
    block[4][0]=1;
    block[2][1]=1;
    block[3][1]=1;
    block[4][1]=1;         
  }

  if (blocktype == 3)
  // 0 0
  // 0 0
  {
    block[3][0]=1;
    block[3][1]=1;
    block[4][0]=1;
    block[4][1]=1;          
  }    

  if (blocktype == 4)
  //   0 0
  // 0 0
  {
    block[4][0]=1;
    block[5][0]=1;
    block[3][1]=1;
    block[4][1]=1;         
  }    
  
  if (blocktype == 5)
  //   0
  // 0 0 0
  {
    block[4][0]=1;
    block[3][1]=1;
    block[4][1]=1;
    block[5][1]=1;       
  }        

  if (blocktype == 6)
  // 0 0
  //   0 0
  {
    block[3][0]=1;
    block[4][0]=1;
    block[4][1]=1;
    block[5][1]=1;         
  }    

  blockrotation = 0;
}

//**********************************************************************************************************************************************************  
boolean space_below()
{   
  for (Ti=15;Ti>=0;Ti--)
  {
    for (Tj=0;Tj<8;Tj++)
    {
       if (block[Tj][Ti])
       {
         if (Ti == 15)
           return false;
         if (pile[Tj][Ti+1])
         {
           return false;
         }      
       }        
    }
  }
  return true;
}

//**********************************************************************************************************************************************************  
boolean space_left2()
{ 
  for (Ti=15;Ti>=0;Ti--)
  {
    for (Tj=0;Tj<8;Tj++)
    {
       if (block[Tj][Ti])
       {
         if (Tj == 0 || Tj == 1)
           return false;
         if (pile[Tj-1][Ti] | pile[Tj-2][Ti])
         {
           return false;
         }      
       }        
    }
  }
  return true;
}

//**********************************************************************************************************************************************************  
boolean space_left3()
{  
  for (Ti=15;Ti>=0;Ti--)
  {
    for (Tj=0;Tj<8;Tj++)
    {
       if (block[Tj][Ti])
       {
         if (Tj == 0 || Tj == 1 ||Tj == 2 )
           return false;
         if (pile[Tj-1][Ti] | pile[Tj-2][Ti]|pile[Tj-3][Ti])
         {
           return false;
         }      
       }        
    }
  }
  return true;
}

//**********************************************************************************************************************************************************  
boolean space_left()
{ 
  for (Ti=15;Ti>=0;Ti--)
  {
    for (Tj=0;Tj<8;Tj++)
    {
       if (block[Tj][Ti])
       {
         if (Tj == 0)
           return false;
         if (pile[Tj-1][Ti])
         {
           return false;
         }      
       }        
    }
  }
  return true;
}

//**********************************************************************************************************************************************************  
boolean space_right()
{ 
  for (Ti=15;Ti>=0;Ti--)
  {
    for (Tj=0;Tj<8;Tj++)
    {
       if (block[Tj][Ti])
       {
         if (Tj == 7)
           return false;
         if (pile[Tj+1][Ti])
         {
           return false;
         }      
       }        
    }
  }
  return true;
}

//**********************************************************************************************************************************************************  
boolean space_right3()
{  
  for (Ti=15;Ti>=0;Ti--)
  {
    for (Tj=0;Tj<8;Tj++)
    {
       if (block[Tj][Ti])
       {
         if (Tj == 7||Tj == 6||Tj == 5)
           return false;
         if (pile[Tj+1][Ti] |pile[Tj+2][Ti] | pile[Tj+3][Ti])
         {
           return false;
         }      
       }        
    }
  }
  return true;
}

//**********************************************************************************************************************************************************  
boolean space_right2()
{ 
  for (Ti=15;Ti>=0;Ti--)
  {
    for (Tj=0;Tj<8;Tj++)
    {
       if (block[Tj][Ti])
       {
         if (Tj == 7 || Tj == 6)
           return false;
         if (pile[Tj+1][Ti] |pile[Tj+2][Ti])
         {
           return false;
         }      
       }        
    }
  }
  return true;
}


//**********************************************************************************************************************************************************  
void LEDRefresh()
{
    int i;
    int k;
	
	phone.clearDisplay();
	
	for(i = 0; i < 6; i++)
	{
		phone.lcd_buffer[LCDWIDTH * i] = 0xFF;
		phone.lcd_buffer[(LCDWIDTH * i) + 26] = 0xFF;
	}
	for(i = 0; i < 8; i++)
		for(k = 0; k < 16; k++)
			if(disp[i][k]) 
			{
				phone.SetPx((i*3)+2, (k*3)+1);
				phone.SetPx((i*3)+3, (k*3)+1);

				phone.SetPx((i*3)+3, (k*3)+2);
				phone.SetPx((i*3)+2, (k*3)+2);

				/*phone.SetPx((i*3)+1, (k*3));
				phone.SetPx((i*3)+2, (k*3));
				phone.SetPx((i*3)+3, (k*3));
				
				phone.SetPx((i*3)+2, (k*3)+1);
				phone.SetPx((i*3)+1, (k*3)+1);
				phone.SetPx((i*3)+1, (k*3)+2);
				
				phone.SetPx((i*3)+3, (k*3)+2);
				phone.SetPx((i*3)+3, (k*3)+1);
				phone.SetPx((i*3)+2, (k*3)+2);*/
			}
	phone.display();
	
  
	 //if (gameoverFlag == true)
	 //{
   /* for(i=0;i<8;i++)
    {      
       byte upper = 0;
       int b;
       for(b = 0;b<8;b++)
       {
         upper <<= 1;
         if (tmpdispUpper[b][i]) upper |= 1;
       }
       
       
       byte lower = 0;
       for(b = 0;b<8;b++)
       {
         lower <<= 1;
         if (tmpdispLower[b][i]) lower |= 1;
       }

            
      if (gameoverFlag == true)
      {  
        elapsedTime = millis() - startTime;

        // Display random pattern for pre-defined period before blanking display
        if (elapsedTime < 2000)
        {            
          /*bi_maxTransferSingle(RED, 1, i,  random(255));
          bi_maxTransferSingle(RED, 2, i,  random(255));
          
          bi_maxTransferSingle(GREEN, 1, i, random(255));
          bi_maxTransferSingle(GREEN, 2, i, random(255));
      
          cnt = cnt + 1;
          
          if (cnt > 80)
          {
            TriggerSound();
            TriggerSound();
            cnt = 0;
          }
        }   
        else
        {
          // clear       
        }  
      
      }
 
    } 
    
    
    
    if (gameoverFlag == false)
    {  
      // For pile - to display orange    
      for(i=0;i<8;i++)
      {      
         byte upper = 0;
         int b;
         for(b = 0;b<8;b++)
         {
           upper <<= 1;
           if (tmppileUpper[b][i]) upper |= 1;
         }
       
       
         byte lower = 0;
         for(b = 0;b<8;b++)
         {
           lower <<= 1;
           if (tmppileLower[b][i]) lower |= 1;
         }

      
        // To alternate color of new block between RED and GREEN
       /*if (selectColor == RED)
        {
          bi_maxTransferSingle(RED, 1, i, lower);
          bi_maxTransferSingle(RED, 2, i, upper);
        }
        else
        {
          bi_maxTransferSingle(GREEN, 1, i, lower);
          bi_maxTransferSingle(GREEN, 2, i, upper);    
        }  

      }         
    }    */
}


