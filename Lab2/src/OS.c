/*
 * OS.c
 *
 */

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "LED.h"

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

static void (*Task1)(void);   // user function
static uint32_t counter1;

int OS_AddPeriodicThread(void(*task)(void), uint32_t period, uint32_t priority) {
	long sr = StartCritical();

	SYSCTL_RCGCTIMER_R |= 0x02;   // 0) activate TIMER1
	Task1 = task;          // user function
	counter1 = 0;
	TIMER1_CTL_R = 0x00000000;    // 1) disable TIMER1A during setup
	TIMER1_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
	TIMER1_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
	TIMER1_TAILR_R = period-1;    // 4) reload value
	TIMER1_TAPR_R = 0;            // 5) bus clock resolution
	TIMER1_ICR_R = 0x00000001;    // 6) clear TIMER1A timeout flag
	TIMER1_IMR_R = 0x00000001;    // 7) arm timeout interrupt
	NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF00FF)| (priority << 13); // 8) priority bit 15-13
	// interrupts enabled in the main program after all devices initialized
	// vector number 37, interrupt number 21
	NVIC_EN0_R = 1<<21;           // 9) enable IRQ 21 in NVIC
	TIMER1_CTL_R = 0x00000001;    // 10) enable TIMER1A
	EndCritical(sr);
}

void Timer1A_Handler(void){
	LED_RED_TOGGLE();
	TIMER1_ICR_R = TIMER_ICR_TATOCINT;  // acknowledge
	(*Task1)();                // execute user task
	counter1++;
	LED_RED_TOGGLE();
}

void OS_ClearPeriodicTime(void) {
	counter1 = 0;
}

uint32_t OS_ReadPeriodicTime(void) {
	return counter1;
}
