#ifndef LED_H
#define LED_H

#include <Arduino.h>

#define LED_PIN 2
#define EXT_LED_PIN 5  // Pin D1 on NodeMCU

#ifdef __cplusplus
extern "C" {
#endif

void led_init(void);
void led_toggle(void); // Toggles internal LED (legacy support)
void led_set(uint8_t state); // Sets internal LED (legacy support)

void led_internal_set(uint8_t state);
void led_external_set(uint8_t state);
void led_internal_toggle(void);
void led_external_toggle(void);

#ifdef __cplusplus
}
#endif

#endif
