/*
 * OS.h
 *
 */

#ifndef INC_OS_H_
#define INC_OS_H_

//******** OS_AddPeriodicThread ***************
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
int OS_AddPeriodicThread(void(*task)(void), uint32_t period, uint32_t priority);

/*
 * Clears the count for the running thread
 * input: none
 * output: none
 */
void OS_ClearPeriodicTime(void);

/* Outputs the number of times the thread has run
 * input: none
 * output: count
 */
uint32_t OS_ReadPeriodicTime(void);

#endif /* INC_OS_H_ */
