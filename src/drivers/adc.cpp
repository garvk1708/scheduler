#include "adc.h"

void adc_init(void) {
    // A0 on ESP8266 doesn't need a pinMode call — it's analog-only by hardware
}

// Returns the raw 10-bit ADC reading from A0 (0–1023).
// Note: analogRead() on ESP8266 takes ~100µs and briefly pauses WiFi processing.
// If you're running WiFi alongside this, consider sampling on a slower interval.
uint16_t adc_read(void) {
    return analogRead(ADC_PIN);
}
