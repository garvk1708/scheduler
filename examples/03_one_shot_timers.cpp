#include <Arduino.h>
#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/uart.h"

Scheduler core;

void processCLI();
void turnOffLED();
void periodicStatus();

Job cliJob(50, INF, &processCLI);
Job statusJob(1000, INF, &periodicStatus);

// One-shot timer job — registered at startup but left stopped.
// Gets configured and started dynamically when the 'trigger' command comes in.
// repeatCount is reset to 1 each time so it fires exactly once per trigger.
Job oneShotOffJob(3000, 1, &turnOffLED);

void setup() {
    custom_uart_init(115200);
    led_init();

    core.add(cliJob);
    core.add(statusJob);
    core.add(oneShotOffJob);

    cliJob.start();
    statusJob.start();
    // oneShotOffJob left stopped — starts only on user command

    Serial.println("\n==============================================");
    Serial.println("   Cooperative Scheduler - One-Shot Timers");
    Serial.println("==============================================");
    Serial.println("Commands:");
    Serial.println("  'trigger X'   - Turn ON external LED for X milliseconds");
    Serial.println("==============================================\n");
}

void loop() {
    core.run();
}

void periodicStatus() {
    Serial.println("[Heartbeat] running...");
}

void turnOffLED() {
    led_external_set(0);
    Serial.println("[TIMER] Expired — external LED off.");
}

void processCLI() {
    char cmd[64];
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strncmp(cmd, "trigger ", 8) == 0) {
            long duration = atol(cmd + 8);
            if (duration <= 0) duration = 1000;

            Serial.printf("[CLI] LED on for %ld ms\n", duration);
            led_external_set(1);

            // Reset and arm the one-shot timer with the requested duration
            oneShotOffJob.intervalMs = duration;
            oneShotOffJob.repeatCount = 1;
            oneShotOffJob.start();
        } else {
            Serial.printf("Unknown command. Try: 'trigger 3000'\n");
        }
    }
}
