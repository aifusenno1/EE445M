/*
 * OS.h
 *
 */

#ifndef INC_OS_H_
#define INC_OS_H_

int OS_AddPeriodicThread(void(*task)(void), uint32_t period, uint32_t priority);

void OS_ClearPeriodicTime(void);

uint32_t OS_ReadPeriodicTime(void);

#endif /* INC_OS_H_ */
