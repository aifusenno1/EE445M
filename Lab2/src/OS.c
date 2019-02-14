/*
 * OS.c
 *
 */

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "LED.h"

void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void StartOS(void);

static void (*Task1)(void);   // user function
static uint32_t counter1;



#define NUMTHREADS  3        // maximum number of threads
#define STACKSIZE   100      // number of 32-bit words in stack

typedef struct tcb {
	uint32_t *sp;
	struct tcb *prev;
	struct tcb *next;
	int id;
	uint32_t sleepCnt;
//	int blocked;
//	uint32_t priority;
} tcbType;

static tcbType tcbs[NUMTHREADS];
static uint32_t threadCnt;
static int nextID = 0;
static tcbType *lastThread;
static tcbType *RunPt;

// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: systick, 80 MHz PLL
// input:  none
// output: none
void OS_Init(void){
  OS_DisableInterrupts();
  PLL_Init(Bus80MHz);         // set processor clock to 80 MHz
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
}


/* Currently, id is used to track the avaliability of a thread slot.
 * Don't know if this will conflict with some other requirement later on
 */
static int findFreeThreadSlot(void) {
	for (int i=0; i<NUMTHREADS; i++) {
		if (tcbs[i].id == -1)
			return i;
	}
	return -1;
}
//******** OS_AddThread ***************
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned long priority) {
	int32_t sr;
	sr = StartCritical();

	if (threadCnt == 0) {
		tcbs[0].next = &tcbs[0];
		tcbs[0].prev = &tcbs[0];
		tcbs[0].id = nextID++;
		tcbs[0].sleepCnt = 0;
		lastThread = &tcbs[0];
		RunPt = &tcbs[0];
	}
	else {
		int slot = findFreeThreadSlot();
		if (slot == -1) return 0;

		// keeping both next and prev helps relinking when a thread dies
		tcbs[slot].next = lastThread->next;
		tcbs[slot].prev = lastThread;
		lastThread->next->prev = &tcbs[slot];  // last thread's prev is the first thread
		lastThread->next = &tcbs[slot];
		lastThread = &tcbs[slot];

		tcbs[slot].id = nextID++;
		tcbs[slot].sleepCnt = 0;
	}

	threadCnt++;


	EndCritical(sr);
	return 1;
}



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
