#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/adc.h"
#include "drivers/uart.h"

// Define Scheduler
Scheduler core;

// Declare Task Functions
void blinkInternalLED();
void sampleSensor();
void handleUARTCommands();

// Create Job Objects (Interval in ms, Repeat Count, Function pointer)
Job blinkJob(1000, INF, &blinkInternalLED);  // Default: slow status blink (1000ms)
Job adcJob(1000, INF, &sampleSensor);         // Sample sensor every 1 second
Job serialJob(50, INF, &handleUARTCommands);   // Check UART commands every 50ms

// Define threshold for the alarm trigger
const uint16_t SENSOR_THRESHOLD = 600;
uint16_t latestSensorValue = 0;

void setup() {
    custom_uart_init(115200);
    led_init();
    adc_init();

    // Register Jobs with the scheduler
    core.add(blinkJob);
    core.add(adcJob);
    core.add(serialJob);

    // Start all jobs
    blinkJob.start();
    adcJob.start();
    serialJob.start();

    Serial.println("\n==============================================");
    Serial.println("   Cooperative Scheduler Alarm System Started");
    Serial.println("==============================================");
    Serial.println("Supported commands:");
    Serial.println("  'led on'      - Turn ON the External LED");
    Serial.println("  'led off'     - Turn OFF the External LED");
    Serial.println("  'led toggle'  - Toggle the External LED");
    Serial.println("  'stats'       - Print Scheduler diagnostics");
    Serial.println("==============================================");

    // Disable WiFi to conserve power and reduce background noise during ADC reads
    WiFi.mode(WIFI_OFF);
    wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
    core.run();
}

// Task 1: Blink internal LED (Serves as status indicator / alarm warning)
void blinkInternalLED() {
    led_internal_toggle();
}

// Task 2: Sample sensor (Read Analog A0)
void sampleSensor() {
    latestSensorValue = adc_read_nonblocking();
    
    // Change blinking interval based on sensor state
    if (latestSensorValue > SENSOR_THRESHOLD) {
        // High reading indicates warning: Rapid warning blink (100ms)
        blinkJob.intervalMs = 100;
        
        // Output alert (throttled by the job's execution frequency)
        Serial.print("[ALERT] Sensor Exceeded Threshold! Value: ");
        Serial.println(latestSensorValue);
    } else {
        // Normal reading: Slow status blink (1000ms)
        blinkJob.intervalMs = 1000;
    }
}

// Task 3: Parse and execute serial commands
void handleUARTCommands() {
    char cmd[64];
    
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strcmp(cmd, "led on") == 0) {
            led_external_set(1);
            Serial.println("-> External LED: ON");
        } 
        else if (strcmp(cmd, "led off") == 0) {
            led_external_set(0);
            Serial.println("-> External LED: OFF");
        }
        else if (strcmp(cmd, "led toggle") == 0) {
            led_external_toggle();
            Serial.println("-> External LED: Toggled");
        }
        else if (strcmp(cmd, "stats") == 0) {
            // Print diagnostics for each registered Job
            Serial.println("\n--- Scheduler Diagnostics & Timing Stats ---");
            Serial.printf("%-12s | %-6s | %-12s | %-12s | %-12s | %-12s | %-12s\n", 
                          "Job Name", "State", "Interval(ms)", "Executions", "Last(us)", "Max(us)", "Avg(us)");
            Serial.println("-----------------------------------------------------------------------------------------");
            
            Job* current = core.getFirstJob();
            int jobIdx = 1;
            while (current != nullptr) {
                const char* name = "Unknown";
                if (current->actionFunction == &blinkInternalLED) name = "BlinkInternal";
                else if (current->actionFunction == &sampleSensor) name = "SampleSensor";
                else if (current->actionFunction == &handleUARTCommands) name = "UARTCommands";

                unsigned long avgRunTime = 0;
                if (current->executionCount > 0) {
                    avgRunTime = current->totalRunTimeUs / current->executionCount;
                }

                Serial.printf("%-12s | %-6s | %-12lu | %-12lu | %-12lu | %-12lu | %-12lu\n",
                              name,
                              current->isRunning ? "RUN" : "STOP",
                              current->intervalMs,
                              current->executionCount,
                              current->lastRunTimeUs,
                              current->maxRunTimeUs,
                              avgRunTime);
                
                current = current->nextJob;
                jobIdx++;
            }
            Serial.println("-----------------------------------------------------------------------------------------\n");
        }
        else {
            Serial.print("-> Unknown command: ");
            Serial.println(cmd);
        }
    }
}
