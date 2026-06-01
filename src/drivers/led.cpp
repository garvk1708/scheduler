#include "led.h"

static uint8_t led_internal_state = 0;
static uint8_t led_external_state = 0;

void led_init(void) {
    pinMode(LED_PIN, OUTPUT);
    pinMode(EXT_LED_PIN, OUTPUT);
    led_internal_set(0);
    led_external_set(0);
}

// Legacy wrappers pointing to internal LED
void led_toggle(void) {
    led_internal_toggle();
}

void led_set(uint8_t state) {
    led_internal_set(state);
}

void led_internal_set(uint8_t state) {
    led_internal_state = state ? 1 : 0;
    // Onboard LED is active-low
    if (led_internal_state == 1) {
        digitalWrite(LED_PIN, LOW);
    } else {
        digitalWrite(LED_PIN, HIGH);
    }
}

void led_external_set(uint8_t state) {
    led_external_state = state ? 1 : 0;
    // External LED is active-high
    if (led_external_state == 1) {
        digitalWrite(EXT_LED_PIN, HIGH);
    } else {
        digitalWrite(EXT_LED_PIN, LOW);
    }
}

void led_internal_toggle(void) {
    led_internal_set(!led_internal_state);
}

void led_external_toggle(void) {
    led_external_set(!led_external_state);
}
