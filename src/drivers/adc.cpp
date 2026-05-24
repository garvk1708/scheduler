#include "adc.h"

void adc_init(void) {
}

uint16_t adc_read_nonblocking(void) {
    return analogRead(ADC_PIN);
}
