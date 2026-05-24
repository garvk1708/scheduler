#include "led.h"

static uint8_t led_current_state = 0;

void led_init(void) {
    pinMode(LED_PIN, OUTPUT);
    led_set(0);
}

void led_toggle(void) {
    led_current_state = !led_current_state;
    led_set(led_current_state);
}

void led_set(uint8_t state) {
    led_current_state = state ? 1 : 0;
    if (led_current_state == 1) {
        digitalWrite(LED_PIN, LOW);
    } else {
        digitalWrite(LED_PIN, HIGH);
    }
}
