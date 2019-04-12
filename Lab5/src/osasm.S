
        .syntax             unified
        .thumb
        .text
        .align 2

        .global  OS_DisableInterrupts
        .global  OS_EnableInterrupts
        .global  StartOS
        .global  SysTick_Handler
        .global  SVC_Handler
        .global  OS_Test

.thumb_func
OS_DisableInterrupts:  .func
        CPSID   I
        BX      LR
       .endfunc

.thumb_func
OS_EnableInterrupts: .func
        CPSIE   I
        BX      LR
       .endfunc

.thumb_func
SysTick_Handler:   .func       // 1) Saves R0-R3,R12,LR,PC,PSR
    CPSID   I                  // 2) Prevent interrupt during switch
    PUSH    {R4-R11}           // 3) Save remaining regs r4-11
#ifdef DEBUG
//	PUSH    {R0,LR}
//	BL		LED_RED_TOGGLE
//	BL		LED_RED_TOGGLE
//	POP		{R0,LR}
#endif
    LDR     R0, =RunPt          // 4) R0=pointer to RunPt, old thread
    LDR     R1, [R0]           //    R1 = RunPt
    STR     SP, [R1]           // 5) Save SP into TCB
    PUSH	{R0, LR}
    BL		threadScheduler	   // get next RunPt
    POP		{R0, LR}
    LDR		R1, [R0]		   // R1 = RunPt (update)
    LDR     SP, [R1]           // 7) new thread SP; SP = RunPt->sp;
#ifdef DEBUG
//    PUSH    {R0,LR}
//	BL		LED_RED_TOGGLE
//	POP		{R0,LR}
#endif
    POP     {R4-R11}           // 8) restore regs r4-11
    CPSIE   I                  // 9) tasks run with interrupts enabled
    BX      LR                 // 10) restore R0-R3,R12,LR,PC,PSR
   .endfunc

// RunPtAddr .field RunPt,32


// starting the OS for the first time (loading the first thread)
// a "fake" stack must have been created for all initially existing threads
.thumb_func
StartOS:	.func
    BL		threadScheduler	   // update RunPt and pcbPt; do not need to save LR, will be overwritten anyways
    LDR     R0, =RunPt
    LDR     R2, [R0]           // R2 = value of RunPt
    LDR     SP, [R2]           // new thread SP; SP = RunPt->sp;
    POP     {R4-R11}           // restore regs r4-11; the pop order just complies the way they are stored in stack
    POP     {R0-R3}            // restore regs r0-3
    POP     {R12}
    ADD     SP, SP, #4         // discard LR from initial stack;
    POP     {LR}               // start location (pop the PC, loaded into LR, so this procedure returns scheduled thread)
    ADD     SP, SP, #4         // discard PSR;
    CPSIE   I                  // Enable interrupts at processor level
    BX      LR                 // start first thread
	.endfunc

.thumb_func
SVC_Handler:	.func			// Saves R0-R3,R12,LR,PC,PSR (top to bottom)
//	PUSH    {R4,LR}
//	BL		LED_RED_TOGGLE
//	POP		{R4,LR}
	LDR  R12,[SP,#24]			// load PC to R12 (the instruction after SVC in user prog)
	LDRH R12,[R12,#-2]			// load mem[PC-2] (the SVC instruction) to R12; SVC instruction is 2 byte
	BIC  R12,#0xFF00			    // &~ clear top bits (extract only the SVC ID)
	LDM  SP,{R0-R3}				// load any parameters from stack into R0-R3 (auto increment SP?)

	// call corresponding OS_XXX
	PUSH    {R4,LR}				// cannot use R0 here, cuz R0 used as return val
	CMP  R12, #0
	BEQ  JOS_Id
	CMP  R12, #1
	BEQ  JOS_Kill
	CMP  R12, #2
	BEQ  JOS_Sleep
	CMP  R12, #3
	BEQ  JOS_Time
	CMP  R12, #4
	BEQ  JOS_AddThread

JOS_Id:		BL	OS_Id
			B   done
JOS_Kill:	BL  OS_Kill
			B   done
JOS_Sleep:	BL  OS_Sleep
			B   done
JOS_Time:	BL  OS_Time
			B   done
JOS_AddThread:	BL  OS_AddThread
				B   done
done:
	POP	 {R4,LR}
	STR R0,[SP]					// store return value
//	PUSH    {R4,LR}
//	BL		LED_RED_TOGGLE
//	POP		{R4,LR}
	BX LR						// restore R0-R3,R12,LR,PC,PSR
	.endfunc

 .end
