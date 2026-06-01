#include <Arduino.h>
#include "my_scheduler.h"

// Instantiate the Cooperative Scheduler
Scheduler core;

// Declare task callbacks
void heartbeatTask();

// Define a Job that runs forever (INF) every 1000 milliseconds
Job heartbeatJob(1000, INF, &heartbeatTask);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\nCooperative Scheduler skeleton ready!");

    // Register tasks
    core.add(heartbeatJob);

    // Start tasks
    heartbeatJob.start();
}

void loop() {
    // Run the scheduler queue traversal
    core.run();
}

// Implement your non-blocking tasks here
void heartbeatTask() {
    Serial.printf("[Uptime: %lu ms] Heartbeat active.\n", millis());
}
