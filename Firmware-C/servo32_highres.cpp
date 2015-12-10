/*
  Elev8 Flight Controller

  Copyright 2015 Parallax Inc

  This work is licensed under a Creative Commons Attribution-ShareAlike 4.0 International License.
  http://creativecommons.org/licenses/by-sa/4.0/

  Written by Jason Dorie
*/

#include <propeller.h>

#include "constants.h"
#include "servo32_highres.h"


struct ServoData {
  long FastPins, SlowPins;
  long MasterLoopDelay, SlowUpdateCounter;
  long Cycles;
  long ServoData[32];		//Servo Pulse Width information
} Data;

//10 clocks is the smallest amount we can wait - everything else is based on that.
//If you're using a different clock speed, your center point will likely need to be adjusted
static const int Scale = 10;

void Servo32_Start(void)
{
  use_cog_driver(servo32_highres_driver);
  load_cog_driver(servo32_highres_driver, &Data);
}


void Servo32_Init( int fastRate )
{
  Data.MasterLoopDelay = Const_ClockFreq / fastRate;
  Data.SlowUpdateCounter = (Const_ClockFreq / 50) / Data.MasterLoopDelay;

  memset(&Data.ServoData[0], 0, sizeof(Data.ServoData) );

  Data.FastPins = 0;
  Data.SlowPins = 0;
}


//Set a PIN index as a high-speed output (250Hz)  
void Servo32_AddFastPin( int Pin )
{
  Data.FastPins |= (1<<Pin);                                            
  Data.SlowPins |= (1<<Pin);	//Fast pins are also updated during the "slow" cycle                                              
}


//Set a PIN index as a standard-speed output (50Hz)  
void Servo32_AddSlowPin( int Pin )
{
  Data.SlowPins |= (1<<Pin);	//Slow pins are ONLY updated during the "slow" cycle
}


void Servo32_Set( int ServoPin, int Width )		// Set Servo value as a raw delay, in 10 clock increments
{
  Data.ServoData[ServoPin] = Width * Scale;		// Servo widths are set in 10ths of a uS, so 8000 = min, 12000 = mid, 16000 = max
}

/*
void Servo32_SetRC( int ServoPin, int Width )	// Set Servo value signed, assuming 12000 is your center
{
	Data.ServoData[ServoPin] = (12000 + Width) * Scale;	// Servo widths are set in 10ths of a uS, so -4000 min, 0 = mid, +4000 = max
}

int Servo32_GetCycles(void)
{
	return Data.Cycles;
}
*/
