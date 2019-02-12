// Timer.c

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "Timer.h"


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

static void (*Task1)(void);   // user function
static void (*Task2)(void);   // user function
static void (*Task3)(void);   // user function

// When concate TimerA and TimerB to use 32 bit, Timer A's control and status bits are used
void Timer1A_Init(void(*task)(void), uint32_t period){
	long sr = StartCritical();

	SYSCTL_RCGCTIMER_R |= 0x02;   // 0) activate TIMER1
	Task1 = task;          // user function
	TIMER1_CTL_R = 0x00000000;    // 1) disable TIMER1A during setup
	TIMER1_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
	TIMER1_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
	TIMER1_TAILR_R = period-1;    // 4) reload value
	TIMER1_TAPR_R = 0;            // 5) bus clock resolution
	TIMER1_ICR_R = 0x00000001;    // 6) clear TIMER1A timeout flag
	TIMER1_IMR_R = 0x00000001;    // 7) arm timeout interrupt
	NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF00FF)|0x00008000; // 8) priority 4
	// interrupts enabled in the main program after all devices initialized
	// vector number 37, interrupt number 21
	NVIC_EN0_R = 1<<21;           // 9) enable IRQ 21 in NVIC
	TIMER1_CTL_R = 0x00000001;    // 10) enable TIMER1A
	EndCritical(sr);
}

/** Timer2A_Init() **
 * Activate TIMER2A to countdown for period seconds
 * Initializes Timer2A for period interrupts
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq), 32 bits
// Outputs: none
 */
void Timer2A_Init(void(*task)(void), uint32_t period){
	
  long sr = StartCritical(); 
	volatile uint32_t delay;
  SYSCTL_RCGCTIMER_R |= 0x04;   // 0) activate TIMER2
	delay = SYSCTL_RCGCTIMER_R;   // allow time to finish activating
	Task2 = task;          // user function
	TIMER2_CTL_R = ~TIMER_CTL_TAEN;    // 1) disable TIMER2A during setup
	TIMER2_CFG_R = 0;    // 2) configure for 32-bit mode
	TIMER2_TAPR_R = 0;            // 5) bus clock resolution
	TIMER2_TAMR_R = TIMER_TAMR_TAMR_PERIOD;   // 3) configure for periodic mode, default down-count settings
	TIMER2_TAILR_R = period-1;    	// 4) reload value
	TIMER2_IMR_R = TIMER_IMR_TATOIM;// arm timeout interrupt
	TIMER2_ICR_R = TIMER_ICR_TATOCINT;    // 6) clear TIMER2A timeout flag

	// Timer 2 interupts
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF); // clear priority
	NVIC_PRI5_R = (NVIC_PRI5_R | 0x80000000); // priority 4
  NVIC_EN0_R = 1 << 23;              // enable interrupt 23 in NVIC
	TIMER2_CTL_R = 0x00000001;    // 10) enable
  EndCritical(sr);
}

/** Timer3A_Init() **
 * Activate TIMER3A to countdown for period seconds
 * Initializes Timer2A for period interrupts
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq), 32 bits
// Outputs: none
 */
void Timer3A_Init(void(*task)(void), uint32_t period){
	long sr;
	sr = StartCritical();
	volatile uint32_t delay;
	SYSCTL_RCGCTIMER_R |= 0x08;   // 0) activate Timer3
	delay = SYSCTL_RCGCTIMER_R;   // allow time to finish activating
	Task3 = task;          // user function
	TIMER3_CTL_R = ~TIMER_CTL_TAEN;    // 1) disable TIMER3A during setup
	TIMER3_CFG_R = 0;    // 2) configure for 32-bit mode
	TIMER3_TAPR_R = 0;            // 5) bus clock resolution
	TIMER3_TAMR_R = TIMER_TAMR_TAMR_PERIOD;   // 3) configure for periodic mode, default down-count settings
	TIMER3_TAILR_R = period - 1;    	// 4) reload value
	TIMER3_IMR_R = TIMER_IMR_TATOIM;// arm timeout interrupt
	TIMER3_ICR_R = TIMER_ICR_TATOCINT;    // 6) clear TIMER3A timeout flag
	// Timer 3 interupts
	NVIC_PRI8_R = (NVIC_PRI8_R&0x00FFFFFF); // clear priority
	NVIC_PRI8_R = (NVIC_PRI8_R | 0x80000000); // priority 4
	NVIC_EN1_R = 1 << 3;              // enable interrupt 35 in NVIC
	TIMER3_CTL_R = 0x00000001;    // 10) enable
	EndCritical(sr);
}



//void Timer1A_Handler(void){
//  Timer1A_Acknowledge();
//  (*Task1)();                // execute user task
//}

void Timer2A_Handler(void){
  Timer2A_Acknowledge();
  (*Task2)();                // execute user task
}

void Timer3A_Handler(void){
   Timer3A_Acknowledge();
  (*Task3)();                // execute user task
}

/******************* Timer1A Methods ****************************/

/** Timer1A_Start() **
 * Restart the Clock (TIMER 1
 */
inline void Timer1A_Start(){
	TIMER1_CTL_R |= TIMER_CTL_TAEN;
}

/** Timer1A_Stop() **
 * Stop the Clock (TIMER 1
 */
inline void Timer1A_Stop(){
	TIMER1_CTL_R &= ~TIMER_CTL_TAEN;
}

/** Timer1_Arm() **
 * Enable interrupts from Timer1
 */
inline void Timer1A_Arm(){
	NVIC_EN0_R = 1 << 21;
}

/** Timer1_Disarm() **
 * Disable interrupts from Timer1
 */
inline void Timer1A_Disarm(){
	NVIC_DIS0_R = 1 << 21;
}

/** Timer1_Acknowledge() **
 * Acknowledge a Timer1A interrupt
 */
inline void Timer1A_Acknowledge(){
	TIMER1_ICR_R = TIMER_ICR_TATOCINT; 
}

/** Timer1_Period() **
 * Reset the period on Timer1
 */
inline void Timer1A_Period(uint32_t period){
	TIMER1_TAILR_R = period - 1; 
}

/******************* Timer2 Methods ****************************/

/**
 * Restart the Clock
 */
inline void Timer2A_Start(){
	TIMER2_CTL_R |= TIMER_CTL_TAEN;
}

/**
 * Stop the Clock 
 */
inline void Timer2A_Stop(){
	TIMER2_CTL_R &= ~TIMER_CTL_TAEN;
}

/**
 * Enable interrupts from Timer2
 */
inline void Timer2A_Arm(){
	NVIC_EN0_R = 1 << 23;
}

/*
 * Disable interrupts from Timer2
 */
inline void Timer2A_Disarm(){
	NVIC_DIS0_R = 1 << 23;
}

/** 
 * Acknowledge a Timer2 interrupt
 */
inline void Timer2A_Acknowledge(){
	TIMER2_ICR_R = TIMER_ICR_TATOCINT; 
}

/** Timer2_Period() **
 * Reset the period on Timer2
 */
inline void Timer2A_Period(uint32_t period){
	TIMER2_TAILR_R = period - 1; 
}

/******************* Timer3 Methods ****************************/

/*
 * Restart the Clock
 */
inline void Timer3A_Start(){
	TIMER3_CTL_R |= TIMER_CTL_TAEN;
}

/*
 * Stop the Clock 
 */
inline void Timer3A_Stop(){
	TIMER3_CTL_R &= ~TIMER_CTL_TAEN;
}

/*
 * Enable interrupts from Timer3.
 */
inline void Timer3A_Arm(){
	NVIC_EN1_R = 1 << 3;
}

/*
 * Disable interrupts from Timer3.
 */
inline void Timer3A_Disarm(){
	NVIC_DIS1_R = 1 << 3;
}

/*
 * Acknowledge a Timer3 interrupt
 */
inline void Timer3A_Acknowledge(){
	TIMER3_ICR_R = TIMER_ICR_TATOCINT; 
}

/*
 * Reset the period on Timer3
 */
inline void Timer3A_Period(uint32_t period){
	TIMER3_TAILR_R = period - 1; 
}
