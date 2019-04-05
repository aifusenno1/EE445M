#/*****************************************************************************/
#/* OSasm.s: low-level OS commands, written in assembly                       */
#/*****************************************************************************/
#Jonathan Valvano/Andreas Gerstlauer, OS Lab 5 solution, 2/28/16


        .syntax             unified
        .thumb
        .text
        .align 2

		.global	OS_Id
        .global  OS_Sleep
		.global	OS_Kill
		.global	OS_Time
		.global	OS_AddThread

.thumb_func
OS_Id:
	SVC		#0
	BX		LR

OS_Kill:
	SVC		#1
	BX		LR

OS_Sleep:
	SVC		#2
	BX		LR

OS_Time:
	SVC		#3
	BX		LR

OS_AddThread:
	SVC		#4
	BX		LR

.end
