#include "led.h"

static uint8_t led_internal_state = 0;
static uint8_t led_external_state = 0;

void led_init(void) {
    pinMode(LED_PIN, OUTPUT);
    pinMode(EXT_LED_PIN, OUTPUT);
    led_internal_set(0);
    led_external_set(0);
}

// Legacy wrappers — kept for backward compatibility with older example code
void led_toggle(void) {
    led_internal_toggle();
}

void led_set(uint8_t state) {
    led_internal_set(state);
}

void led_internal_set(uint8_t state) {
    led_internal_state = state ? 1 : 0;
    // Onboard LED on NodeMCU is active-low (LOW = on)
    digitalWrite(LED_PIN, led_internal_state ? LOW : HIGH);
}

void led_external_set(uint8_t state) {
    led_external_state = state ? 1 : 0;
    // External LED is wired active-high
    digitalWrite(EXT_LED_PIN, led_external_state ? HIGH : LOW);
}

void led_internal_toggle(void) {
    led_internal_set(!led_internal_state);
}

void led_external_toggle(void) {
    led_external_set(!led_external_state);
}
