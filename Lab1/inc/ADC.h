/*
 * ADC.h
 *
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_

void ADC_Open(uint32_t channelNum);

uint16_t ADC_In(void);

void ADC_Collect(uint32_t channelNum, uint32_t fs, uint16_t buffer[], uint32_t numberOfSamples);

#endif /* INC_ADC_H_ */
