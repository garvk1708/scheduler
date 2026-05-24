#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "my_scheduler.h"

#include "drivers/led.h"
#include "drivers/adc.h"
#include "drivers/uart.h"

Scheduler core;

void blinkLED();
void readAnalog();
void handleSerial();

Job blinkJob(500, INF, &blinkLED);
Job adcJob(2000, INF, &readAnalog);
Job serialJob(50, INF, &handleSerial);

void setup() {
    uart_init(115200);
    led_init();
    adc_init();

    core.add(blinkJob);
    core.add(adcJob);
    core.add(serialJob);
    
    blinkJob.start();
    adcJob.start();
    serialJob.start();
    
    wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
    core.run();
}

void blinkLED() {
    led_toggle();
}

void readAnalog() {
    uint16_t val = adc_read_nonblocking();
    Serial.println(val);
}

void handleSerial() {
    char cmd[64];
    
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strcmp(cmd, "led on") == 0) {
            blinkJob.stop();
            led_set(1);
        } 
        else if (strcmp(cmd, "led off") == 0) {
            blinkJob.stop();
            led_set(0);
        }
        else if (strcmp(cmd, "led toggle") == 0) {
            led_toggle();
        }
        else if (strcmp(cmd, "blink on") == 0) {
            blinkJob.start();
        }
    }
}
