
        .syntax             unified
        .thumb
        .text
        .align 2

        .global  RunPt
        .global  OS_DisableInterrupts
        .global  OS_EnableInterrupts
        .global  StartOS
        .global  SysTick_Handler

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
    LDR     R0, =RunPt          // 4) R0=pointer to RunPt, old thread
    LDR     R1, [R0]           //    R1 = RunPt
    STR     SP, [R1]           // 5) Save SP into TCB
    LDR     R1, [R1,#4]        // 6) R1 = RunPt->next
    STR     R1, [R0]           //    RunPt = R1
    LDR     SP, [R1]           // 7) new thread SP; SP = RunPt->sp;
    POP     {R4-R11}           // 8) restore regs r4-11
    CPSIE   I                  // 9) tasks run with interrupts enabled
    BX      LR                 // 10) restore R0-R3,R12,LR,PC,PSR
   .endfunc

// RunPtAddr .field RunPt,32


// starting the OS for the first time (loading the first thread)
// a "fake" stack must have been created for all initially existing threads
.thumb_func
StartOS:	.func
    LDR     R0, =RunPt      // the next thread to be run
    LDR     R2, [R0]           // R2 = value of RunPt
    LDR     SP, [R2]           // new thread SP; SP = RunPt->stackPointer;
    POP     {R4-R11}           // restore regs r4-11; the pop order just complies the way they are stored in stack
    POP     {R0-R3}            // restore regs r0-3
    POP     {R12}
    ADD     SP, SP, #4         // discard LR from initial stack;
    POP     {LR}               // start location (pop the PC, loaded into LR, so this procedure returns to the next thread)
    ADD     SP, SP, #4         // discard PSR;
    CPSIE   I                  // Enable interrupts at processor level
    BX      LR                 // start first thread
	.endfunc

    .end
