#include <Arduino.h>
#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/uart.h"

Scheduler core;

void fastTask();
void loggerTask();
void blockerTask();
void shellTask();

Job fastJob(100, INF, &fastTask);
Job loggerJob(1000, INF, &loggerTask);
Job blockerJob(8000, INF, &blockerTask);
Job shellJob(50, INF, &shellTask);

void setup() {
    custom_uart_init(115200);
    led_init();

    core.add(fastJob);
    core.add(loggerJob);
    core.add(blockerJob);
    core.add(shellJob);

    fastJob.start();
    loggerJob.start();
    blockerJob.start();
    shellJob.start();

    Serial.println("\n==============================================");
    Serial.println("   Cooperative Scheduler - CPU Profiling");
    Serial.println("==============================================");
    Serial.println("Every 8s, BlockerTask calls delay(1000).");
    Serial.println("Type 'stats' to see the impact on other tasks.");
    Serial.println("==============================================\n");
}

void loop() {
    core.run();
}

void fastTask() {
    led_internal_toggle();
}

void loggerTask() {
    Serial.println("[Status] Heartbeat on schedule.");
}

// This task intentionally blocks for 1 second to demonstrate what happens
// when a cooperative task doesn't yield. During the delay, all other tasks
// are frozen — their Max(us) values in 'stats' will show the gap.
void blockerTask() {
    Serial.println("\n!!! [BLOCKER] Calling delay(1000) — watch other tasks freeze !!!");
    unsigned long t = millis();
    delay(1000);
    Serial.printf("!!! [BLOCKER] Done. Held CPU for %lu ms !!!\n\n", millis() - t);
}

void shellTask() {
    char cmd[64];
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strcmp(cmd, "stats") == 0) {
            Serial.println("\n--- Scheduler Timing Diagnostics ---");
            Serial.printf("%-15s | %-12s | %-12s | %-12s | %-12s | %-12s\n", 
                          "Task", "Interval(ms)", "Runs", "Last(us)", "Max(us)", "Avg(us)");
            Serial.println("--------------------------------------------------------------------------------------");
            
            Job* current = core.getFirstJob();
            while (current != nullptr) {
                const char* name = "?";
                if (current->actionFunction == &fastTask)    name = "FastTask(100ms)";
                else if (current->actionFunction == &loggerTask)  name = "LoggerTask(1s)";
                else if (current->actionFunction == &blockerTask) name = "Blocker(8s)";
                else if (current->actionFunction == &shellTask)   name = "UARTShell(50ms)";

                unsigned long avg = current->executionCount > 0
                    ? current->totalRunTimeUs / current->executionCount
                    : 0;

                Serial.printf("%-15s | %-12lu | %-12lu | %-12lu | %-12lu | %-12lu\n",
                              name,
                              current->intervalMs,
                              current->executionCount,
                              current->lastRunTimeUs,
                              current->maxRunTimeUs,
                              avg);
                
                current = current->nextJob;
            }
            Serial.println("--------------------------------------------------------------------------------------");
            Serial.println("Look at Blocker(8s) Max(us) — it'll be around 1,000,000.");
            Serial.println("FastTask's max will also spike because it couldn't run during the block.\n");
        }
    }
}
