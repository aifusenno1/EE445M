/*
 * ADC.h
 *
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_

/*
 * Open ADC0SS3 to collect 1 piece of data
 * ADC0
 * SS3
 * Timer2A interrupt (periodic mode)
 * 10000 Hz sampling rate on 80 MHz clock
 * input: the channel number to open
 * output: none
 */
void ADC_Init(uint32_t channelNum);

/*
 * read the previously recorded ADC value
 * input:  none
 * output: the previously read ADC value
 */
uint16_t ADC_In(void);

/*
 * Open ADC0SS3 with the specified channelNum at interrupting frequency fs, take numberOfSamples of samples,
 * store them in buffer, then disable the interrupt
 * ADC0
 * SS3
 * Timer2A triggered
 * inputs:  channelNum: the channel number to open
 * 			fs: the interrupting frequency
 * 			buffer: pointer to the array that stores data
 * 			numberOfSamples: number of samples to collect
 */
void ADC_Collect(uint32_t channelNum, uint32_t fs, void (*task)(uint32_t));

#endif /* INC_ADC_H_ */
