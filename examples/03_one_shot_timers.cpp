#include <Arduino.h>
#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/uart.h"

// Instantiate the scheduler
Scheduler core;

// Declare task functions
void processCLI();
void turnOffLED();
void periodicStatus();

// Define Jobs:
// cliJob runs continuously (INF) every 50ms to read commands
Job cliJob(50, INF, &processCLI);

// statusJob runs continuously every 1000ms
Job statusJob(1000, INF, &periodicStatus);

// oneShotOffJob is a software timer configured to run exactly ONCE (1 repeat)
// It starts in a stopped/inactive state and will be started dynamically.
Job oneShotOffJob(3000, 1, &turnOffLED);

void setup() {
    custom_uart_init(115200);
    led_init();

    // Register all tasks
    core.add(cliJob);
    core.add(statusJob);
    core.add(oneShotOffJob); // Note: It is stopped initially since repeatCount is not yet active

    // Start baseline tasks
    cliJob.start();
    statusJob.start();

    Serial.println("\n==============================================");
    Serial.println("   Cooperative Scheduler - One-Shot Software Timers");
    Serial.println("==============================================");
    Serial.println("Supported commands:");
    Serial.println("  'trigger X'   - Turn ON external LED for X milliseconds");
    Serial.println("==============================================\n");
}

void loop() {
    core.run();
}

// Baseline Task: Prints status heartbeat
void periodicStatus() {
    Serial.println("[Heartbeat] System running cooperatively...");
}

// Dynamic Action: Turn off external LED and log it
void turnOffLED() {
    led_external_set(0);
    Serial.println("[TIMER EVENT] Timer expired: External LED turned OFF.");
}

// Parser Task: Monitors commands to trigger the one-shot timer
void processCLI() {
    char cmd[64];
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strncmp(cmd, "trigger ", 8) == 0) {
            // Parse duration
            long duration = atol(cmd + 8);
            if (duration <= 0) duration = 1000; // default 1 second

            Serial.printf("[CLI] Command received. Turning ON External LED for %ld ms...\n", duration);
            
            // Turn ON the LED physically
            led_external_set(1);

            // Configure the timer job dynamically
            oneShotOffJob.intervalMs = duration;
            oneShotOffJob.repeatCount = 1; // Run exactly once
            oneShotOffJob.start();         // Start timer
        } else {
            Serial.printf("Unknown command. Try: 'trigger 3000'\n");
        }
    }
}
