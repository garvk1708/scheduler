#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/adc.h"
#include "drivers/uart.h"

// Instantiate the Cooperative Scheduler
Scheduler core;

// Define custom task callbacks
void taskBlink();
void taskReadSensor();
void taskProcessCLI();

// Instantiate Job objects (Interval in ms, Repeat count, Callback)
Job blinkJob(500, INF, &taskBlink);
Job sensorJob(2000, INF, &taskReadSensor);
Job cliJob(50, INF, &taskProcessCLI);

void setup() {
    // Initialize hardware drivers
    custom_uart_init(115200);
    led_init();
    adc_init();

    // Register tasks with the scheduler
    core.add(blinkJob);
    core.add(sensorJob);
    core.add(cliJob);

    // Start tasks
    blinkJob.start();
    sensorJob.start();
    cliJob.start();

    // Turn off WiFi to reduce noise & save power
    WiFi.mode(WIFI_OFF);
    wifi_set_sleep_type(LIGHT_SLEEP_T);
    
    Serial.println("Scheduler skeleton ready for custom code!");
}

void loop() {
    // Continuously execute registered cooperative tasks
    core.run();
}

void taskBlink() {
    led_toggle(); // Toggles internal onboard LED
}

void taskReadSensor() {
    uint16_t val = adc_read_nonblocking(); // Reads from Analog input (A0)
    Serial.print("Sensor reading: ");
    Serial.println(val);
}

void taskProcessCLI() {
    char cmd[64];
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strcmp(cmd, "led on") == 0) {
            blinkJob.stop();
            led_set(1);
            Serial.println("Blink stopped, LED ON");
        }
        else if (strcmp(cmd, "led off") == 0) {
            blinkJob.stop();
            led_set(0);
            Serial.println("Blink stopped, LED OFF");
        }
        else if (strcmp(cmd, "led toggle") == 0) {
            led_toggle();
            Serial.println("LED Toggled");
        }
        else if (strcmp(cmd, "blink on") == 0) {
            blinkJob.start();
            Serial.println("Blink Resumed");
        }
        else {
            Serial.print("Unknown command: ");
            Serial.println(cmd);
        }
    }
}
