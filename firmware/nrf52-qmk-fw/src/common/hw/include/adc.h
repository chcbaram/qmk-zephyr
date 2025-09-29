#ifndef ADC_H_
#define ADC_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "hw_def.h"

#ifdef _USE_HW_ADC


#define ADC_MAX_CH    HW_ADC_MAX_CH


bool    adcInit(void);
bool    adcIsInit(void);
int32_t adcRead(uint8_t ch);
int32_t adcRead8(uint8_t ch);
int32_t adcRead10(uint8_t ch);
int32_t adcRead12(uint8_t ch);
int32_t adcRead16(uint8_t ch);
uint8_t adcGetRes(uint8_t ch);
float   adcReadVoltage(uint8_t ch);
float   adcConvVoltage(uint8_t ch, uint32_t adc_value);


#endif

#ifdef __cplusplus
}
#endif

#endif