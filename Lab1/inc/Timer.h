// Timer.h
// Runs on LM4F120/TM4C123
// Use Timer0A in periodic mode to request interrupts at a particular
// period.
// Daniel Valvano
// September 11, 2013

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015
  Program 7.5, example 7.6

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
#include <stdint.h>

void Timer1A_Init(void(*task)(void), uint32_t period);
void Timer2A_Init(void(*task)(void), uint32_t period);
void Timer3A_Init(void(*task)(void), uint32_t period);
void Timer0B_Init(void(*task)(void), uint32_t period);

/******************* Timer1A Methods ****************************/
void  Timer1A_Start(void);
void  Timer1A_Stop(void);
void  Timer1A_Arm(void);
void  Timer1A_Disarm(void);
void  Timer1A_Acknowledge(void);
void  Timer1A_Period(uint32_t period);

/******************* Timer2A Methods ****************************/
void  Timer2A_Start(void);
void  Timer2A_Stop(void);
void  Timer2A_Arm(void);
void  Timer2A_Disarm(void);
void  Timer2A_Acknowledge(void);
void  Timer2A_Period(uint32_t period);

/******************* Timer3A Methods ****************************/
void  Timer3A_Start(void);
void  Timer3A_Stop(void);
void  Timer3A_Arm(void);
void  Timer3A_Disarm(void);
void  Timer3A_Acknowledge(void);
void  Timer3A_Period(uint32_t period);
