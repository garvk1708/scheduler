#include <Arduino.h>
#include "my_scheduler.h"
#include "drivers/led.h"

// Instantiate the scheduler
Scheduler core;

// Declare task function
void blinkTask();

// Define a Job that runs forever (INF) every 500ms
Job blinkJob(500, INF, &blinkTask);

void setup() {
    led_init(); // Initialize LED pin

    // Register job to scheduler
    core.add(blinkJob);

    // Start execution of the job
    blinkJob.start();
}

void loop() {
    // Run the scheduler loop
    core.run();
}

// Simple non-blocking toggle function
void blinkTask() {
    led_toggle(); // Toggles internal onboard LED
}
