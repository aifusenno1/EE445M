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
 * Timer2A interrupt (one-shot mode)
 * 10000 Hz sampling rate on 80 MHz clock
 * (This function does not match what is required by Lab 2, and will be redesigned)
 * input: the channel number to open
 * output: none
 */
void ADC_Open(uint32_t channelNum);

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
 * (This function does not match what is required in Lab2. Will be redesigned.)
 * inputs:  channelNum: the channel number to open
 * 			fs: the interrupting frequency
 * 			buffer: pointer to the array that stores data
 * 			numberOfSamples: number of samples to collect
 */
void ADC_Collect(uint32_t channelNum, uint32_t fs, uint16_t buffer[], uint32_t numberOfSamples);

#endif /* INC_ADC_H_ */
