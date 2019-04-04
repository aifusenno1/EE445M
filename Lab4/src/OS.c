/*
 * OS.c
 *
 */


#include "OS.h"
#include "tm4c123gh6pm.h"
#include "LED.h"
#include "PLL.h"
#include "Serial.h"
#include "ST7735.h"

#define PE0  (*((volatile unsigned long *)0x40024004))
#define PE1  (*((volatile unsigned long *)0x40024008))
#define PE2  (*((volatile unsigned long *)0x40024010))
#define PE3  (*((volatile unsigned long *)0x40024020))

#define TIME_1MS    80000
#define TIME_2MS    (2*TIME_1MS)
#define TIME_500US  (TIME_1MS/2)
#define TIME_250US  (TIME_1MS/5)
#define OS_PERIOD   TIME_1MS  // period of OS_Timer, in unit of 12.5ns (cycles)

#define STACKSIZE   500      // number of 32-bit words in stack

static unsigned long OS_Timer;	   // in unit of 1ms by default

int32_t Stacks[NUMTHREADS][STACKSIZE];
tcbType *RunPt;
static tcbType tcbs[NUMTHREADS];
static uint32_t threadCnt;
static int nextID = 0;
static tcbType *lastThread;

void StartOS(void);
static void os_timer_init(void);


// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: systick, 80 MHz PLL
// input:  none
// output: none
void OS_Init(void){
  OS_DisableInterrupts();	  // disable all processor interrupt; will be enabled in OS_Launch
  PLL_Init(Bus80MHz);         // set processor clock to 80 MHz
  LED_Init();
  Serial_Init();
  LCD_Init();

  os_timer_init();

  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
}

// notice R13 (MSP/PSP) not stored in stack
static void setInitialStack(int i, void (*thread_starting_addr)(void)){
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer, initially pointing to the bottom (above all registers)
  Stacks[i][STACKSIZE-1] = 0x01000000;   // thumb bit (PSR)
  Stacks[i][STACKSIZE-2] = (int32_t) thread_starting_addr;  // PC
  Stacks[i][STACKSIZE-3] = 0x14141414;   // R14 (LR)
  Stacks[i][STACKSIZE-4] = 0x12121212;   // R12  SP
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
		tcbs[0].priority = priority;
		setInitialStack(0, task);
		lastThread = &tcbs[0];
		RunPt = &tcbs[0];
	}
	else {
		int slot = findFreeThreadSlot();
		if (slot == -1)  {
			EndCritical(sr);
			return 0;
		}

		// keeping both next and prev helps relinking when a thread dies
		tcbs[slot].next = lastThread->next;
		tcbs[slot].prev = lastThread;
		lastThread->next->prev = &tcbs[slot];  // last thread's prev is the first thread
		lastThread->next = &tcbs[slot];
		lastThread = &tcbs[slot];

		setInitialStack(slot, task);
		tcbs[slot].id = nextID++;
		tcbs[slot].state = ACTIVE;
		tcbs[slot].priority = priority;
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
	tcbType * pt = RunPt;
	tcbType * endPt;  // endPt is the last thread to check in the Linked List
	// whether this thread is killed
	if (RunPt->state == FREE) {
		endPt = RunPt->prev;
	}
	else {
		endPt = RunPt;
	}
	tcbType * bestPt;
	int maxPri = 255;

	do {
		pt = pt->next;
		if (pt->state == ACTIVE && pt->priority < maxPri) {
			maxPri = pt->priority;
			bestPt = pt;
		}
	} while (pt != endPt);
	RunPt = bestPt;

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
	OS_Timer++;
	// decrement all sleeping threads
	for (int i=0; i<NUMTHREADS; i++) {
		if (tcbs[i].state == SLEEP) {
			if (--tcbs[i].sleepTimeLeft == 0) {
				tcbs[i].state = ACTIVE;
			}
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
	if (stop >= start)
		return stop - start;
	else
		return  4294967296 - (start - stop);
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
	semaPt->start = 0;
	semaPt->end = 0;
}

/* Wait can only be called by main thread, because suspend (thread switch) only applies to main threads */
// ******** OS_Wait ************
// decrement semaphore
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt) {
	OS_DisableInterrupts();
	semaPt->value = semaPt->value - 1;
	if (semaPt->value < 0) {
		RunPt->state = BLOCKED;
		RunPt->blocked = semaPt;
		semaPt->waiters[semaPt->end] = RunPt;  // add to waiters list
		semaPt->end = (semaPt->end + 1) % NUMTHREADS;
		OS_EnableInterrupts();
		OS_Suspend();
	}
	OS_EnableInterrupts();
}

// ******** OS_Signal ************
// increment semaphore
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt) {
	unsigned long sr = StartCritical();
	tcbType *pt = RunPt;
	semaPt->value = semaPt->value + 1;
	if (semaPt->value <= 0) {
		semaPt->waiters[semaPt->start]->state = ACTIVE;		// release the first blocked thread
		semaPt->start = (semaPt->start + 1) % NUMTHREADS;
	}
	EndCritical(sr);
}


// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt) {
	OS_DisableInterrupts();
	while (semaPt->value == 0) {
		RunPt->state = BLOCKED;
		RunPt->blocked = semaPt;
		semaPt->waiters[semaPt->end] = RunPt;  // add to waiters list
		semaPt->end = (semaPt->end + 1) % NUMTHREADS;
		OS_EnableInterrupts();
		OS_Suspend();
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
    if (semaPt->value == 0) {
    	semaPt->waiters[semaPt->start]->state = ACTIVE;		// release the first blocked thread
    	semaPt->start = (semaPt->start + 1) % NUMTHREADS;
    }
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
	return ret;
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
	/*
	 * Notice here, cannot just use semaphore.value to check whether it's full or not
	 * because in Get, DataNum may be decremented, then it's switched out or interrupted by Put
	 * before the data is actually recorded. Thus we need to make sure FIFO is never full
	 */
	uint32_t volatile *nextPutPt;
	nextPutPt = putPt+1;
	if(nextPutPt == &fifo[OS_FIFO_SIZE]){
		nextPutPt = &fifo[0];  // wrap
	}
	if(nextPutPt == getPt){
		return 0;      // Failed, fifo full; Since cannot wait here (in ISR)
	}
	*putPt = data;
	putPt = nextPutPt;
	OS_Signal(&ff_DataNum);		// increment current data number
	return 1;
}

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data
unsigned long OS_Fifo_Get(void) {
	OS_Wait(&ff_DataNum);	    // decrement current data number; if empty, spin or block
								// wait until other consumer thread finishes, then lock
	unsigned long data = *getPt;
	OS_DisableInterrupts();     // make the getPt increment process atomic
	getPt++;
	if (getPt == &fifo[OS_FIFO_SIZE])
		getPt = &fifo[0];
	OS_EnableInterrupts();
	return data;
}

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
long OS_Fifo_Size(void) {
	if(putPt < getPt){
		return ((unsigned short)(putPt-getPt+(OS_FIFO_SIZE*sizeof(uint32_t)))/sizeof(uint32_t));
	}
	return ((unsigned short)(putPt-getPt)/sizeof(uint32_t));
}


#define PERIODIC_NUM 2
static void (*periodic_tasks[PERIODIC_NUM])(void);   // user function
static int periodic_num = 0;
static uint32_t periodic_counters[PERIODIC_NUM];
static uint32_t periodic_periods[PERIODIC_NUM];

unsigned long NumSamples;
unsigned long maxJitter1;   // in 0.1us units
unsigned long maxJitter2;
unsigned long jitter1Histogram[JITTERSIZE]={0,};
unsigned long jitter2Histogram[JITTERSIZE]={0,};

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
		periodic_periods[periodic_num] = period;
		if (periodic_num == 0) {
			maxJitter1 = 0;
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
			maxJitter2 = 0;
			SYSCTL_RCGCTIMER_R |= 0x01;   // 0) activate TIMER0
			TIMER0_CTL_R = 0x00000000;    // 1) disable TIMER0A during setup
			TIMER0_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
			TIMER0_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
			TIMER0_TAILR_R = period-1;    // 4) reload value
			TIMER0_TAPR_R = 0;            // 5) bus clock resolution
			TIMER0_ICR_R = 0x00000001;    // 6) clear TIMER0A timeout flag
			TIMER0_IMR_R = 0x00000001;    // 7) arm timeout interrupt

			NVIC_PRI4_R = (NVIC_PRI4_R&0x00FFFFFF)| (priority << 29); // 8) priority
			// interrupts enabled in the main program after all devices initialized
			// vector number 35, interrupt number 19
			NVIC_EN0_R = 1<<19;           // 9) enable IRQ 19 in NVIC
			TIMER0_CTL_R = 0x00000001;    // 10) enable TIMER0A
		}
		periodic_num++;
		EndCritical(sr);
		return 1;
	} else {
		EndCritical(sr);
		return 0;
	}
}

void print_jitter(void) {
//	ST7735_Message(1,0,"Jitter 1 = ", maxJitter1);
//	ST7735_Message(1,1,"Jitter 2 = ", maxJitter2);
	printf("Periodic Task 1 jitter (0.1 us): %u\n\r", maxJitter1);
	printf("Periodic Task 2 jitter (0.1 us): %u\n\r", maxJitter2);
}

void Timer1A_Handler(void){
	static unsigned long lastTime;
	unsigned long jitter;
	TIMER1_ICR_R = TIMER_ICR_TATOCINT;  // acknowledge
	unsigned long thisTime;

	if (NumSamples < RUNLENGTH) {
		thisTime= OS_Time();       // current time, 12.5 ns

		periodic_tasks[0]();                // execute user task
		periodic_counters[0]++;
		if(periodic_counters[0]>1){    // ignore timing of first interrupt
			unsigned long diff = OS_TimeDifference(lastTime, thisTime);
			if (diff > periodic_periods[0])
				jitter = (diff-periodic_periods[0]+4)/8;  // in 0.1 usec
			else
				jitter = (periodic_periods[0]-diff+4)/8;  // in 0.1 usec
			if(jitter > maxJitter1)
				maxJitter1 = jitter; // in usec
			// jitter should be 0
			if(jitter >= JITTERSIZE)
				jitter = JITTERSIZE-1;
			jitter1Histogram[jitter]++;
		}
		lastTime = thisTime;
	}
}

void Timer0A_Handler(void){
	static unsigned long lastTime;
	unsigned long jitter;
	TIMER0_ICR_R = TIMER_ICR_TATOCINT;  // acknowledge
	unsigned long thisTime;
	if (NumSamples < RUNLENGTH) {
		thisTime = OS_Time();       // current time, 12.5 ns
		periodic_tasks[1]();                // execute user task
		periodic_counters[1]++;
		if(periodic_counters[1]>1){    // ignore timing of first interrupt
			unsigned long diff = OS_TimeDifference(lastTime, thisTime);
			if (diff > periodic_periods[1])
				jitter = (diff-periodic_periods[1]+4)/8;  // in 0.1 usec
			else
				jitter = (periodic_periods[1]-diff+4)/8;  // in 0.1 usec
			if(jitter > maxJitter2)
				maxJitter2 = jitter; // in usec
			// jitter should be 0
			if(jitter >= JITTERSIZE)
				jitter = JITTERSIZE-1;
			jitter2Histogram[jitter]++;
		}
		lastTime = thisTime;
	}
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
	unsigned long sr = StartCritical();
	if (GPIO_PORTF_RIS_R & 0x10) {  // if PF4 pressed
		GPIO_PORTF_IM_R &= ~0x10;	// disarm interrupt on PF4, debounce purpose
		if (lastPF4) {             	// 0x10 means it was previously released, negative logic
			sw1_task();
		}
		// debounce required on both press and release
		int ret = OS_AddThread(sw1_debounce, 128, 1);  // for debounce purpose, priority for switch tasks need to be high
		if (ret == 0) {  // failed, arm right away			 // so that it can be scheduled right away
			GPIO_PORTF_ICR_R = 0x10;
			GPIO_PORTF_IM_R |= 0x10;
		}
	}
	if (GPIO_PORTF_RIS_R & 0x01) { // if PF0 pressed
		GPIO_PORTF_IM_R &= ~0x01;
		if (lastPF0) {
			sw2_task();
		}
		int ret = OS_AddThread(sw2_debounce, 128, 1);  // for debounce purpose, priority for switch tasks need to be high
		if (ret == 0) {  // failed, arm right away			 // so that it can be scheduled right away
			GPIO_PORTF_ICR_R = 0x01;
			GPIO_PORTF_IM_R |= 0x01;
		}
	}
	EndCritical(sr);
}
