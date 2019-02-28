/*
 * OS.c
 *
 */


#include <OS.h>
#include "tm4c123gh6pm.h"
#include "LED.h"
#include "PLL.h"
#include "Serial.h"
#include "ST7735.h"


void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void StartOS(void);
static void os_timer_init(void);

#define PE0  (*((volatile unsigned long *)0x40024004))
#define PE1  (*((volatile unsigned long *)0x40024008))
#define PE2  (*((volatile unsigned long *)0x40024010))
#define PE3  (*((volatile unsigned long *)0x40024020))

#define TIME_1MS    80000
#define TIME_2MS    (2*TIME_1MS)
#define TIME_500US  (TIME_1MS/2)
#define TIME_250US  (TIME_1MS/5)
#define OS_PERIOD   TIME_1MS  // period of OS_Timer, in unit of 12.5ns (cycles)

#define NUMTHREADS  10        // maximum number of threads
#define STACKSIZE   100      // number of 32-bit words in stack

unsigned long OS_Timer;	   // in unit of 1ms by default

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
  OS_DisableInterrupts();	  // disable all processor interrupt; will be enabled in OS_Launch
  PLL_Init(Bus80MHz);         // set processor clock to 80 MHz
  Serial_Init();
  LED_Init();
  LCD_Init();
  os_timer_init();

  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
}

// notice R13 (MSP/PSP) not stored in stack
static void setInitialStack(int i, void (*thread_starting_addr)(void)){
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer, initially pointing to the bottom
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
//	Serial_println("%u", threadCnt);
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
  StartOS();                   // start on the first task. enable processor interrupt
}

//******** OS_Id ***************
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero
unsigned long OS_Id(void) {
	return RunPt->id;
}

// schedules the next thread to run
void threadScheduler(void) {
	do {
		RunPt = RunPt->next;
	} while (RunPt->state == SLEEP);
#ifdef DEBUG
//	LED_BLUE_TOGGLE();
#endif
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
// **Interrupt must be enabled entering OS_Suspend
void OS_Suspend(void) {
	NVIC_ST_CURRENT_R = 0; 			// clear current count
	NVIC_INT_CTRL_R |= 1 << 26;     // sets the PENDSTSET bit, force systick handler
}

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime) {
	OS_DisableInterrupts();
	RunPt->sleepTimeLeft = sleepTime;   // sleep time in 1ms, the unit of OS_Timer
	RunPt->state = SLEEP;
	OS_EnableInterrupts();
	OS_Suspend();
}

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void) {
	OS_DisableInterrupts();
	// update last thread if this is the last thread
	if (lastThread == RunPt)
		lastThread = RunPt->prev;
	RunPt->state = FREE;
	RunPt->prev->next = RunPt->next;
	RunPt->next->prev = RunPt->prev;
	OS_EnableInterrupts();
	threadCnt--;
	OS_Suspend();
}


// the OS internal timer
// uses Timer 3
// input: period in unit of 12.5ns
static void os_timer_init() {
	unsigned long sr = StartCritical();
	SYSCTL_RCGCTIMER_R |= 0x08;   // 0) activate TIMER3
	TIMER3_CTL_R = 0x00000000;    // 1) disable TIMER3A during setup
	TIMER3_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
	TIMER3_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
	TIMER3_TAILR_R = OS_PERIOD-1;    // 4) reload value
	TIMER3_TAPR_R = 0;            // 5) bus clock resolution
	TIMER3_ICR_R = 0x00000001;    // 6) clear TIMER3A timeout flag
	TIMER3_IMR_R = 0x00000001;    // 7) arm timeout interrupt
	NVIC_PRI8_R = (NVIC_PRI8_R&0x00FFFFFF)|0x20000000; // 8) priority 1
	// vector number 51, interrupt number 35
	NVIC_EN1_R = 1<<(35-32);      // 9) enable IRQ 35 in NVIC
	TIMER3_CTL_R = 0x00000001;    // 10) enable TIMER3A
    OS_Timer = 0;
	EndCritical(sr);
}

void Timer3A_Handler(void){
	TIMER3_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER3A timeout
//	  LED_GREEN_ON();
//	Serial_println("1 %u", NVIC_ST_CURRENT_R);
	OS_Timer++;
	// decrement all sleeping threads
	for (int i=0; i<NUMTHREADS; i++) {
		if (tcbs[i].state == SLEEP) {
			if (--tcbs[i].sleepTimeLeft == 0)
				tcbs[i].state = ACTIVE;
		}
	}
//	  LED_GREEN_OFF();
}


// ******** OS_Time ************
// return the system time
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as
//   this function and OS_TimeDifference have the same resolution and precision
unsigned long OS_Time(void) {
	return OS_Timer * OS_PERIOD + (OS_PERIOD - 1 - TIMER3_TAR_R);  // the right part is the elapsed cycles that yet counted into OS_Timer
}
// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as
//   this function and OS_Time have the same resolution and precision
unsigned long OS_TimeDifference(unsigned long start, unsigned long stop) {
	unsigned long dif = stop - start;
	if (dif >= 0)
		return dif;
	else
		return 0xFFFFFFFF - dif + 1;
}

// ******** OS_ClearMsTime ************
// sets the system time to zero (from Lab 1)
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void) {
	OS_Timer = 0;
	TIMER3_TAR_R = OS_PERIOD - 1;	// reload the counter
}

// ******** OS_MsTime ************
// reads the current time in msec (from Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// It is ok to make the resolution to match the first call to OS_AddPeriodicThread
unsigned long OS_MsTime(void) {
	return OS_Timer * (OS_PERIOD / TIME_1MS);
}

// ******** OS_InitSemaphore ************
// initialize semaphore
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, long value) {
	semaPt->value = value;
}

/* Wait can only be called by main thread, because suspend (thread switch) only applies to main threads */
// ******** OS_Wait ************
// decrement semaphore
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt) {
	OS_DisableInterrupts();   // why not save I bit here?
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
	unsigned long sr = StartCritical();  // why save I bit here?
	semaPt->value = semaPt->value + 1;
	EndCritical(sr);
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
    unsigned long sr = StartCritical();  // why save I bit here?
	semaPt->value = 1;
    EndCritical(sr);
}

static unsigned long mailbox;
static Sema4Type mb_DataValid;
static Sema4Type mb_BoxFree;
// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void) {
	OS_InitSemaphore(&mb_DataValid, 0);  // initialized to zero meaning the MailBox is empty. When DataValid equals one it means the mailbox has data in it placed by Consumer that has not been read by the Display.
	OS_InitSemaphore(&mb_BoxFree, 1);    // initialized to one meaning the MailBox is free. When BoxFree equals zero it means the mailbox contains data that has not yet been received
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received
void OS_MailBox_Send(unsigned long data) {
	OS_bWait(&mb_BoxFree);  // wait until mailbox is free, then mark it full
	mailbox = data;
	OS_bSignal(&mb_DataValid);  // signal that mailbox is filled
}

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty
unsigned long OS_MailBox_Recv(void) {
	unsigned long ret;
	OS_bWait(&mb_DataValid);  // wait until mailbox data is filled
	ret = mailbox;
	OS_bSignal(&mb_BoxFree);  // signal that mailbox is now free
}

#define OS_FIFO_SIZE 16
volatile uint32_t *getPt;
volatile uint32_t *putPt;
static uint32_t fifo[OS_FIFO_SIZE];
static Sema4Type ff_DataNum;
//static Sema4Type ff_mutex;		// mutex not needed, because only one consumer
// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(unsigned long size) {
	putPt = getPt = &fifo[0];
	OS_InitSemaphore(&ff_DataNum, 0);
//	OS_InitSemaphore(&ff_mutex, 1);		// initially released
}

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers
//  this function can not disable or enable interrupts
int OS_Fifo_Put(unsigned long data) {
	// only one background producer thread, no critical section
	if (ff_DataNum.value == OS_FIFO_SIZE) {  // full
		return 0;  // data lost
	}
	*putPt = data;
	putPt++;
	if (putPt == &fifo[OS_FIFO_SIZE])
		putPt = &fifo[0];
	OS_Signal(&ff_DataNum);		// increment current data number
	return 1;
}

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data
unsigned long OS_Fifo_Get(void) {
	OS_Wait(&ff_DataNum);	   // decrement current data number; if empty, spin or block
//	OS_Wait(&ff_mutex);		   // wait until other consumer thread finishes, then lock
	unsigned long data = *getPt;
	getPt++;
	if (getPt == &fifo[OS_FIFO_SIZE])
		getPt = &fifo[0];
//	OS_Signal(&ff_mutex);	   // release lock
	return data;
}

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
long OS_Fifo_Size(void);


#define PERIODIC_NUM 1
static void (*periodic_tasks[PERIODIC_NUM])(void);   // user function
static int periodic_num = 0;
static uint32_t periodic_counters[PERIODIC_NUM];

//******** OS_AddPeriodicThread ***************
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// This task does not have a Thread ID
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field
//           determines the relative priority of these four threads
int OS_AddPeriodicThread(void(*task)(void), uint32_t period, uint32_t priority) {
	long sr = StartCritical();
	if (periodic_num < PERIODIC_NUM) {
		periodic_tasks[periodic_num] = task;
		periodic_counters[periodic_num] = 0;
		if (periodic_num == 0) {
			SYSCTL_RCGCTIMER_R |= 0x02;   // 0) activate TIMER1
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
		} else if (periodic_num == 1) {

		}
		periodic_num++;
		EndCritical(sr);
		return 1;
	} else {
		EndCritical(sr);
		return 0;
	}
}


void Timer1A_Handler(void){
	TIMER1_ICR_R = TIMER_ICR_TATOCINT;  // acknowledge
	periodic_tasks[0]();                // execute user task
	periodic_counters[0]++;
}

static void (*sw1_task)(void);
static void (*sw2_task)(void);
static int sw1_pri;
static int sw2_pri;
#define PF4 			(*((volatile unsigned long *)0x40025040))
#define PF0 			(*((volatile unsigned long *)0x40025004))
static int lastPF4, lastPF0;


//******** OS_AddSW1Task ***************
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), unsigned long priority) {
	long sr = StartCritical();
	if (!sw1_task) {
		sw1_task = task;
		sw1_pri = priority;
		unsigned long volatile delay;
		SYSCTL_RCGCGPIO_R |= 0x00000020; 	// (a) activate clock for port F
		delay = SYSCTL_RCGCGPIO_R;
		GPIO_PORTF_LOCK_R = GPIO_LOCK_KEY;		// unlock port F
		GPIO_PORTF_CR_R |= 0x10;           // allow changes to PF4
		GPIO_PORTF_DIR_R &= ~0x10;    		// (c) make PF4 in (built-in button)
		GPIO_PORTF_AFSEL_R &= ~0x10;  		//     disable alt funct on PF4
		GPIO_PORTF_DEN_R |= 0x10;     		//     enable digital I/O on PF4
		GPIO_PORTF_PCTL_R &= ~0x000F0000; // configure PF4 as GPIO
		GPIO_PORTF_AMSEL_R = 0;       		//     disable analog functionality on PF
		GPIO_PORTF_PUR_R |= 0x10;     		//     enable weak pull-up on PF4
		GPIO_PORTF_IS_R &= ~0x10;     		// (d) PF4 is edge-sensitive
		GPIO_PORTF_IBE_R |= 0x10;     		//     PF4 is on both edges
											// debounce requires 2 edges: press and release
		GPIO_PORTF_ICR_R = 0x10;      		// (e) clear flag4
		GPIO_PORTF_IM_R |= 0x10;      		// (f) arm interrupt on PF4
		NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF) | ((priority & 0x07) << 21);  // set up priority
		NVIC_EN0_R = 0x40000000;      		// (h) enable interrupt 30 in NVIC
		lastPF4 = PF4 & 0x10;				// initially high
		EndCritical(sr);
		return 1;
	} else {
		EndCritical(sr);
		return 0;
	}
}

//******** OS_AddSW2Task ***************
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field
//           determines the relative priority of these four threads
int OS_AddSW2Task(void(*task)(void), unsigned long priority) {
	long sr = StartCritical();
	if (!sw2_task) {
		sw2_task = task;
		sw2_pri = priority;
		unsigned long volatile delay;
		SYSCTL_RCGCGPIO_R |= 0x00000020; 	// (a) activate clock for port F
		delay = SYSCTL_RCGCGPIO_R;
		GPIO_PORTF_LOCK_R = GPIO_LOCK_KEY;		// unlock port F
		GPIO_PORTF_CR_R |= 0x01;           // allow changes to PF0
		GPIO_PORTF_DIR_R &= ~0x01;    // (c) make PF0 in (built-in button)
		GPIO_PORTF_AFSEL_R &= ~0x01;  		//     disable alt funct on PF0
		GPIO_PORTF_DEN_R |= 0x01;     		//     enable digital I/O on PF0
		GPIO_PORTF_PCTL_R &= ~0x0000000F; // configure PF0 as GPIO
		GPIO_PORTF_AMSEL_R = 0;       		//     disable analog functionality on PF
		GPIO_PORTF_PUR_R |= 0x01;     		//     enable weak pull-up on PF0
		GPIO_PORTF_IS_R &= ~0x01;     		// (d) PF0 is edge-sensitive
		GPIO_PORTF_IBE_R |= 0x01;     		//     PF0 is both edges

		GPIO_PORTF_ICR_R = 0x01;      		// (e) clear flag0
		GPIO_PORTF_IM_R |= 0x01;      		// (f) arm interrupt on PF0
		NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF) | ((priority & 0x07) << 21);  // set up priority
		NVIC_EN0_R = 0x40000000;      		// (h) enable interrupt 30 in NVIC
		lastPF0 = PF0 & 0x01;
		EndCritical(sr);
		return 1;
	} else {
		EndCritical(sr);
		return 0;
	}
}

static void sw1_debounce(void) {
	OS_Sleep(10);
	lastPF4 = PF4 & 0x10;		// lastPF4 reflects the state of switch after debounce
	GPIO_PORTF_ICR_R = 0x10;
	GPIO_PORTF_IM_R |= 0x10;
//	LED_GREEN_TOGGLE();
	OS_Kill();	// only one time usage, so kill right away
}

static void sw2_debounce(void) {
	OS_Sleep(10);
	lastPF0 = PF0 & 0x01;
	GPIO_PORTF_ICR_R = 0x01;
	GPIO_PORTF_IM_R |= 0x01;
	OS_Kill();
}

void GPIOPortF_Handler(void) {  // negative logic
	if (GPIO_PORTF_RIS_R & 0x10) {  // if PF4 pressed
#ifdef DEBUG
//		LED_GREEN_TOGGLE();
//		LED_GREEN_TOGGLE();
#endif
		GPIO_PORTF_IM_R &= ~0x10;	// disarm interrupt on PF4, debounce purpose
		if (lastPF4) {             	// 0x10 means it was previously released, negative logic
			sw1_task();
		}
		// debounce required on both press and release
		int ret = OS_AddThread(sw1_debounce, 128, sw1_pri);  // for debounce purpose, priority for switch tasks need to be high
		if (ret == 0) {  // failed, arm right away			 // so that it can be scheduled right away
			GPIO_PORTF_ICR_R = 0x10;
			GPIO_PORTF_IM_R |= 0x10;
		}
#ifdef DEBUG
//		LED_GREEN_TOGGLE();
#endif
	}
	if (GPIO_PORTF_RIS_R & 0x01) { // if PF0 pressed
		GPIO_PORTF_IM_R &= ~0x01;
		if (lastPF0) {
			sw2_task();
		}
		int ret = OS_AddThread(sw2_debounce, 128, sw2_pri);  // for debounce purpose, priority for switch tasks need to be high
		if (ret == 0) {  // failed, arm right away			 // so that it can be scheduled right away
			GPIO_PORTF_ICR_R = 0x01;
			GPIO_PORTF_IM_R |= 0x01;
		}
	}
}
