// Lab2.c
// Runs on LM4F120/TM4C123
// Real Time Operating System for Labs 2 and 3
// Lab2 Part 1: Testmain1 and Testmain2
// Lab2 Part 2: Testmain3 Testmain4  and main
// Lab3: Testmain5 Testmain6, Testmain7, and main (with SW2)

// Jonathan W. Valvano 2/20/17, valvano@mail.utexas.edu
// EE445M/EE380L.12
// You may use, edit, run or distribute this file
// You are free to change the syntax/organization of this file

// LED outputs to logic analyzer for OS profile
// PF1 is preemptive thread switch
// PF2 is periodic task, samples PD3
// PF3 is SW1 task (touch PF4 button)

// Button inputs
// PF0 is SW2 task (Lab3)
// PF4 is SW1 button input

// Analog inputs
// PD3 Ain3 sampled at 2k, sequencer 3, by DAS software start in ISR
// PD2 Ain5 sampled at 250Hz, sequencer 0, by Producer, timer tigger

#include "OS.h"
#include "tm4c123gh6pm.h"
#include "ST7735.h"
#include <string.h>
#include "Serial.h"
#include "LED.h"
#include "ff.h"


#define PE0  (*((volatile unsigned long *)0x40024004))
#define PE1  (*((volatile unsigned long *)0x40024008))
#define PE2  (*((volatile unsigned long *)0x40024010))
#define PE3  (*((volatile unsigned long *)0x40024020))

uint32_t NumCreated;

void PortE_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x10;       // activate port E
  while(( SYSCTL_RCGCGPIO_R&0x10)==0){};
  GPIO_PORTE_CR_R = 0x2F;           // allow changes to PE3-0
  GPIO_PORTE_AFSEL_R &= ~0x0F;   // disable alt funct on PE3-0
  GPIO_PORTE_PCTL_R = ~0x0000FFFF;
  GPIO_PORTE_AMSEL_R &= ~0x0F;;      // disable analog functionality on PF
  GPIO_PORTE_DEN_R |= 0x0F;     // enable digital I/O on PE3-0
  GPIO_PORTE_DIR_R = 0x0F;    // make PE3-0 output heartbeats  (PE is opposite from PF?)
}

//******** IdleTask  ***************
// foreground thread, runs when no other work needed
// never blocks, never sleeps, never dies
// inputs:  none
// outputs: none
unsigned long Idlecount=0;
void IdleTask(void){
  while(1) {
	PE0 ^= 0x01;
    Idlecount++;        // debugging
  }
}


void interpreter(void);    // just a prototype, link to your interpreter

const char inFilename[] = "test.txt";   // 8 characters or fewer
const char outFilename[] = "out.txt";   // 8 characters or fewer
static FATFS g_sFatFs;
FIL Handle,Handle2;
FRESULT MountFresult;
FRESULT Fresult;
unsigned char buffer[512];
#define MAXBLOCKS 100

void filesystem(void) {
	UINT successfulreads, successfulwrites;
	uint8_t c, x, y;
	MountFresult = f_mount(&g_sFatFs, "", 0);
	if(MountFresult){
		ST7735_OutString("f_mount error");
	}
	OS_Kill();
}

void OS_Test(void);

void button(void) {
	OS_Test();
	OS_Kill();
}

//************SW1Push*************
// Called when SW1 Button pushed
// background threads execute once and return
void SW1Push(void){
    NumCreated += OS_AddThread(&button,128,1);  // start a 2 second run

}
//************SW2Push*************
// Called when SW2 Button pushed
// background threads execute once and return
void SW2Push(void){
}


int main(void){        // realmain
  OS_Init();           // initialize, disable interrupts
  PortE_Init();

  //*******attach background tasks***********
  OS_AddSW1Task(&SW1Push,2);    // PF4, SW1
  OS_AddSW2Task(&SW2Push,3);   // PF0

  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&filesystem,128,1);
  NumCreated += OS_AddThread(&interpreter,128,2);
  NumCreated += OS_AddThread(&IdleTask,128,7);  // runs when nothing useful to do

  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}
