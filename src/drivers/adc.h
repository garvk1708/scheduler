#ifndef ADC_H
#define ADC_H

#include <Arduino.h>

#define ADC_PIN A0

#ifdef __cplusplus
extern "C" {
#endif

void adc_init(void);
uint16_t adc_read_nonblocking(void);

#ifdef __cplusplus
}
#endif

#endif
