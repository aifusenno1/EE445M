/*
 * OS.c
 *
 */


#include <OS.h>
#include "tm4c123gh6pm.h"
#include "LED.h"
#include "PLL.h"
#include "Serial.h"

void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void SysTick_Handler(void);
void StartOS(void);

#define NUMTHREADS  10        // maximum number of threads
#define STACKSIZE   100      // number of 32-bit words in stack

static int32_t Stacks[NUMTHREADS][STACKSIZE];


tcbType *RunPt;

static tcbType tcbs[NUMTHREADS];
static uint32_t threadCnt;
static int nextID = 0;
static tcbType *lastThread;


// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: systick, 80 MHz PLL
// input:  none
// output: none
void OS_Init(void){
  OS_DisableInterrupts();
  PLL_Init(Bus80MHz);         // set processor clock to 80 MHz
  Serial_Init();

  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
}

// notice R13 (MSP/PSP) not stored in stack
static void setInitialStack(int i, void (*thread_starting_addr)(void)){
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer
  Stacks[i][STACKSIZE-1] = 0x01000000;   // thumb bit (PSR)
  Stacks[i][STACKSIZE-2] = (int32_t) thread_starting_addr;  // PC
  Stacks[i][STACKSIZE-3] = 0x14141414;   // R14 (LR)
  Stacks[i][STACKSIZE-4] = 0x12121212;   // R12
  Stacks[i][STACKSIZE-5] = 0x03030303;   // R3
  Stacks[i][STACKSIZE-6] = 0x02020202;   // R2
  Stacks[i][STACKSIZE-7] = 0x01010101;   // R1
  Stacks[i][STACKSIZE-8] = 0x00000000;   // R0
  Stacks[i][STACKSIZE-9] = 0x11111111;   // R11
  Stacks[i][STACKSIZE-10] = 0x10101010;  // R10
  Stacks[i][STACKSIZE-11] = 0x09090909;  // R9
  Stacks[i][STACKSIZE-12] = 0x08080808;  // R8
  Stacks[i][STACKSIZE-13] = 0x07070707;  // R7
  Stacks[i][STACKSIZE-14] = 0x06060606;  // R6
  Stacks[i][STACKSIZE-15] = 0x05050505;  // R5
  Stacks[i][STACKSIZE-16] = 0x04040404;  // R4
}

/* Currently, id is used to track the avaliability of a thread slot.
 * Don't know if this will conflict with some other requirement later on
 */
static int findFreeThreadSlot(void) {
	for (int i=0; i<NUMTHREADS; i++) {
		if (tcbs[i].state == FREE)
			return i;
	}
	return -1;
}
//******** OS_AddThread ***************
// add a foregound thread to the scheduler
// Inputs: pointer to a void-void foreground task
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
		tcbs[0].state = ACTIVE;
		setInitialStack(0, task);
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

		setInitialStack(slot, task);
		tcbs[slot].id = nextID++;
		tcbs[slot].state = ACTIVE;
	}

	threadCnt++;

	EndCritical(sr);
	return 1;
}


//******** OS_Launch ***************
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(uint32_t theTimeSlice){
  NVIC_ST_RELOAD_R = theTimeSlice - 1; // reload value
  NVIC_ST_CTRL_R = 0x00000007; // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}

// schedules the next thread to run
static void scheduler(void) {
	do {
		RunPt = RunPt->next;
	} while (RunPt->state == SLEEP);
}

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
//void OS_Sleep(unsigned long sleepTime) {
//
//}

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void) {
	NVIC_ST_CURRENT_R = 0; 			// clear current count
	NVIC_INT_CTRL_R |= 1 << 26;     // sets the PENDSTSET bit, force systick handler
}


// ******** OS_InitSemaphore ************
// initialize semaphore
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, long value) {
	semaPt->value = value;
}

// ******** OS_Wait ************
// decrement semaphore
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt) {
	OS_DisableInterrupts();
	while (semaPt->value == 0) {
		OS_EnableInterrupts();
		OS_Suspend();				// run thread switch
		OS_DisableInterrupts();
	}
	semaPt->value = semaPt->value - 1;
	OS_EnableInterrupts();
}

// ******** OS_Signal ************
// increment semaphore
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt) {
	OS_DisableInterrupts();
	semaPt->value = semaPt->value + 1;
	OS_EnableInterrupts();
}


// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt) {
//	OS_DisableInterrupts();
//	semaPt->value = semaPt->value - 1;
//	if (semaPt->value < 0) {
//		RunPt->blocked = semaPt;
//		RunPt->state = BLOCKED;
//		OS_EnableInterrupts();
//		OS_Suspend();				// run thread switch
//	}
//	OS_EnableInterrupts();
	OS_DisableInterrupts();
	while (semaPt->value == 0) {    // if it's 0, need to wait for other thread to signal it (set it)
		OS_EnableInterrupts();		// if it's 1, meaning already signaled (or maybe initially not acquired)
		OS_Suspend();				// run thread switch
		OS_DisableInterrupts();
	}
	semaPt->value = 0;    // write zero back to it, prepared for usage next time
	OS_EnableInterrupts();

}

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt) {
	OS_DisableInterrupts();
	semaPt->value = 1;
	OS_EnableInterrupts();
}


static unsigned long mailbox;
static Sema4Type * mb_DataValid;
static Sema4Type * mb_BoxFree;
// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void) {
	OS_InitSemaphore(mb_DataValid, 0);  // initialized to zero meaning the MailBox is empty. When DataValid equals one it means the mailbox has data in it placed by Consumer that has not been read by the Display.
	OS_InitSemaphore(mb_BoxFree, 1);    // initialized to one meaning the MailBox is free. When BoxFree equals zero it means the mailbox contains data that has not yet been received
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received
void OS_MailBox_Send(unsigned long data) {
	OS_bWait(mb_BoxFree);  // wait until mailbox is free, then mark it full
	mailbox = data;
	OS_bSignal(mb_DataValid);  // signal that mailbox is filled
}

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty
unsigned long OS_MailBox_Recv(void) {
	unsigned long ret;
	OS_bWait(mb_DataValid);  // wait until mailbox data is filled
	ret = mailbox;
	OS_bSignal(mb_BoxFree);  // signal that mailbox is now free
}



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
