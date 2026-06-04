#include <Arduino.h>
#include "my_scheduler.h"

Scheduler core;

void heartbeatTask();

// Runs forever, fires every 1000ms
Job heartbeatJob(1000, INF, &heartbeatTask);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\nScheduler ready.");

    core.add(heartbeatJob);
    heartbeatJob.start();
}

void loop() {
    core.run();
}

void heartbeatTask() {
    Serial.printf("[%lu ms] alive\n", millis());
}
