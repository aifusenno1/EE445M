
  .syntax unified
  .thumb
  .section .text,"ax"
  .balign 4

  .global PID_stm32
  .global IntTerm
  .global PrevError

Err      .req R0    				// 1st function input: Error
Coeff    .req R1    				// 2nd fct input: Address of coefficient table
Kd       .req R1
Ki       .req R2
Kp       .req R3

Out      .req R4
Result   .req R2
Integ    .req R5
PrevErr  .req R12

/*******************************************************************************
* Function Name  : DoPID
* Description    : PID in ASM, Error computed outside the routine
* Input          : Error: difference between reference and measured value
*                  Coeff: pointer to the coefficient table
* Output         : None
* Return         : PID output (command)
*******************************************************************************/

.thumb_func
PID_stm32:  .func

  PUSH {R4, R5, R9}

  LDR R12, =IntTerm
  LDR R9, =PrevError

  LDRH Kp, [Coeff, #0]  		// Load Kp
  LDRH Ki, [Coeff, #2]  		// Load Ki
  LDRH Kd, [Coeff, #4]  		// Load Kd and destroy Coeff
  LDRH Integ, [R12, #0]  		// Last Integral Term
  LDRH PrevErr, [R9, #0]  		// Previous Error

  MLA Integ, Ki, Err, Integ   	// IntTerm += Ki*error
  MLA Out, Kp, Err, Integ      	// Output = (Kp * error) + InTerm
  SUBS PrevErr, Err, PrevErr    // PrevErr now holds DeltaError = Error - PrevError
  MLA Result, Kd, PrevErr, Out  // Output += Kd * DeltaError

  LDR R12, =IntTerm
  STRH Integ, [R12, #0]       	// Write back InTerm
  STRH Err, [R9, #0]         	// Write back PrevError

  MOV R0, Result
  UXTH R0, R0
  POP {R4, R5, R9}
  BX LR
  .endfunc

  .end

/******************* (C) COPYRIGHT 2009  STMicroelectronics *****END OF FILE****/
