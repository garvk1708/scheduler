#include <Arduino.h>
#include "my_scheduler.h"
#include "drivers/led.h"

Scheduler core;

void blinkTask();

Job blinkJob(500, INF, &blinkTask);

void setup() {
    led_init();

    core.add(blinkJob);
    blinkJob.start();
}

void loop() {
    core.run();
}

void blinkTask() {
    led_toggle();
}
