#ifndef LED_H
#define LED_H

#include <Arduino.h>

#define LED_PIN 2

#ifdef __cplusplus
extern "C" {
#endif

void led_init(void);
void led_toggle(void);
void led_set(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif
