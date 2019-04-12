/*
 * ADC.h
 *
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_
#include <stdint.h>

/*
 * Open ADC0SS3
 * ADC0
 * SS3
 * Software triggered
 * input: the channel number to open
 * output: none
 */
void ADC_Init(uint32_t channelNum);

/*
 * read ADC value
 * input:  none
 * output: the ADC value
 */
uint16_t ADC_In(void);

/*
 * ADC0
 * SS2
 * Timer2A triggered
 * inputs:  channelNum: the channel number to open
 * 			fs: the interrupting frequency
 * 			task: the task to run
 */
void ADC_Collect(uint32_t channelNum, uint32_t fs, void (*task)(uint32_t));

#endif /* INC_ADC_H_ */
