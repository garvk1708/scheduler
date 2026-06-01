#include <Arduino.h>
#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/uart.h"

Scheduler core;

void fastTask();
void loggerTask();
void blockerTask();
void shellTask();

// Set up tasks:
Job fastJob(100, INF, &fastTask);         // Runs every 100ms
Job loggerJob(1000, INF, &loggerTask);    // Runs every 1000ms
Job blockerJob(8000, INF, &blockerTask);  // Blocker task runs every 8 seconds
Job shellJob(50, INF, &shellTask);        // Shell CLI runs every 50ms

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
    Serial.println("   Cooperative Scheduler - Timing & CPU Profiling");
    Serial.println("==============================================");
    Serial.println("This demo shows the impact of blocking tasks.");
    Serial.println("Every 8 seconds, 'BlockerTask' will block the CPU");
    Serial.println("for 1 second using delay(1000).");
    Serial.println("Type 'stats' to see how this affects other tasks!");
    Serial.println("==============================================\n");
}

void loop() {
    core.run();
}

// Fast running task (100ms)
void fastTask() {
    led_internal_toggle();
}

// Periodic logger (1000ms)
void loggerTask() {
    Serial.println("[Status] Heartbeat running on schedule.");
}

// BAD task simulating blocking logic: blocks CPU for 1000ms!
void blockerTask() {
    Serial.println("\n!!! [BLOCKER] Warning: Simulating a 1-second block using delay(1000) !!!");
    unsigned long startMs = millis();
    
    // Simulate bad blocking code (delay)
    delay(1000);
    
    unsigned long durationMs = millis() - startMs;
    Serial.printf("!!! [BLOCKER] Done. Blocked CPU for %lu ms. Notice other tasks froze! !!!\n\n", durationMs);
}

// Console handler
void shellTask() {
    char cmd[64];
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strcmp(cmd, "stats") == 0) {
            Serial.println("\n--- Scheduler Timing Diagnostics ---");
            Serial.printf("%-15s | %-12s | %-12s | %-12s | %-12s | %-12s\n", 
                          "Task Name", "Interval(ms)", "Executions", "Last(us)", "Max(us)", "Avg(us)");
            Serial.println("--------------------------------------------------------------------------------------");
            
            Job* current = core.getFirstJob();
            while (current != nullptr) {
                const char* name = "Unknown";
                if (current->actionFunction == &fastTask) name = "FastTask(100ms)";
                else if (current->actionFunction == &loggerTask) name = "LoggerTask(1s)";
                else if (current->actionFunction == &blockerTask) name = "Blocker(8s)";
                else if (current->actionFunction == &shellTask) name = "UARTShell(50ms)";

                unsigned long avgRunTime = 0;
                if (current->executionCount > 0) {
                    avgRunTime = current->totalRunTimeUs / current->executionCount;
                }

                Serial.printf("%-15s | %-12lu | %-12lu | %-12lu | %-12lu | %-12lu\n",
                              name,
                              current->intervalMs,
                              current->executionCount,
                              current->lastRunTimeUs,
                              current->maxRunTimeUs,
                              avgRunTime);
                
                current = current->nextJob;
            }
            Serial.println("--------------------------------------------------------------------------------------");
            Serial.println("Notice that 'Blocker(8s)' reports a massive Max execution time (~1,000,000 us).");
            Serial.println("During the block, 'FastTask' fails to execute, introducing latency jitter.\n");
        }
    }
}
