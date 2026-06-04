#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/adc.h"
#include "drivers/uart.h"

// IR sensor module wired to D2 (GPIO4). Most cheap modules pull this LOW when triggered.
#define IR_SENSOR_PIN 4

Scheduler core;

void blinkHeartbeat();
void readIRSensor();
void readTemperatureSensor();
void handleUARTShell();

Job heartbeatJob(1000, INF, &blinkHeartbeat);
Job irSensorJob(100, INF, &readIRSensor);
Job tempSensorJob(1500, INF, &readTemperatureSensor);
Job uartShellJob(50, INF, &handleUARTShell);

volatile bool irTriggered = false;
volatile bool tempHigh = false;
uint16_t latestTempRaw = 0;

// Raw ADC threshold for the temperature alarm (0–1023 range, tune to your sensor)
const uint16_t TEMP_ALERT_THRESHOLD = 700;

void setup() {
    custom_uart_init(115200);
    led_init();
    adc_init();

    pinMode(IR_SENSOR_PIN, INPUT);

    core.add(heartbeatJob);
    core.add(irSensorJob);
    core.add(tempSensorJob);
    core.add(uartShellJob);

    heartbeatJob.start();
    irSensorJob.start();
    tempSensorJob.start();
    uartShellJob.start();

    Serial.println("\n========================================================");
    Serial.println("   Cooperative Scheduler - Dual Sensor & LED Example");
    Serial.println("========================================================");
    Serial.println("Hardware configuration:");
    Serial.printf("  - Internal LED : GPIO%d (D4) - Onboard Heartbeat\n", LED_PIN);
    Serial.printf("  - External LED : GPIO%d (D1) - Alarm Output\n", EXT_LED_PIN);
    Serial.printf("  - IR Sensor    : GPIO%d (D2) - Proximity Input\n", IR_SENSOR_PIN);
    Serial.printf("  - Temp Sensor  : A0 (ADC0)   - Analog Thermal Input\n");
    Serial.println("--------------------------------------------------------");
    Serial.println("Serial commands:");
    Serial.println("  'stats'        - Live task timing breakdown");
    Serial.println("  'led toggle'   - Manually toggle the external LED");
    Serial.println("========================================================\n");

    // Turn off WiFi — it interferes with analogRead() on the ESP8266 ADC
    WiFi.mode(WIFI_OFF);
    wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
    core.run();
}

// Heartbeat blink. Normally slow (1s), switches to a fast panic flash (150ms)
// if either sensor is in alert state. The interval change takes effect immediately
// on the next scheduler pass.
void blinkHeartbeat() {
    led_internal_toggle();
    heartbeatJob.intervalMs = (irTriggered || tempHigh) ? 150 : 1000;
}

// IR sensor — most modules pull the output LOW on detection.
// Only prints/changes LED state on transitions, not on every poll.
void readIRSensor() {
    bool currentVal = (digitalRead(IR_SENSOR_PIN) == LOW);
    
    if (currentVal != irTriggered) {
        irTriggered = currentVal;
        if (irTriggered) {
            Serial.println("[EVENT] IR: obstacle detected");
            led_external_set(1);
        } else {
            Serial.println("[EVENT] IR: cleared");
            if (!tempHigh) led_external_set(0);
        }
    }
}

// Reads A0. analogRead() on ESP8266 takes ~100µs and pauses WiFi briefly,
// which is fine here since we disabled WiFi in setup().
void readTemperatureSensor() {
    latestTempRaw = adc_read();
    
    if (latestTempRaw > TEMP_ALERT_THRESHOLD) {
        if (!tempHigh) {
            tempHigh = true;
            Serial.printf("[ALERT] Temp high — raw ADC: %d (threshold: %d)\n", latestTempRaw, TEMP_ALERT_THRESHOLD);
            led_external_set(1);
        }
    } else {
        if (tempHigh) {
            tempHigh = false;
            Serial.printf("[OK] Temp back to normal — raw ADC: %d\n", latestTempRaw);
            if (!irTriggered) led_external_set(0);
        }
    }
}

void handleUARTShell() {
    char cmd[64];
    
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strcmp(cmd, "stats") == 0) {
            Serial.println("\n--- Scheduler Task Stats ---");
            Serial.printf("%-15s | %-6s | %-12s | %-12s | %-12s | %-12s | %-12s\n", 
                          "Task", "State", "Interval(ms)", "Runs", "Last(us)", "Max(us)", "Avg(us)");
            Serial.println("---------------------------------------------------------------------------------------------");
            
            Job* current = core.getFirstJob();
            while (current != nullptr) {
                const char* name = "?";
                if (current->actionFunction == &blinkHeartbeat)        name = "Heartbeat";
                else if (current->actionFunction == &readIRSensor)     name = "IRSensor";
                else if (current->actionFunction == &readTemperatureSensor) name = "TempSensor";
                else if (current->actionFunction == &handleUARTShell)  name = "UARTShell";

                unsigned long avg = current->executionCount > 0
                    ? current->totalRunTimeUs / current->executionCount
                    : 0;

                Serial.printf("%-15s | %-6s | %-12lu | %-12lu | %-12lu | %-12lu | %-12lu\n",
                              name,
                              current->isRunning ? "RUN" : "STOP",
                              current->intervalMs,
                              current->executionCount,
                              current->lastRunTimeUs,
                              current->maxRunTimeUs,
                              avg);
                
                current = current->nextJob;
            }
            Serial.println("---------------------------------------------------------------------------------------------\n");
        }
        else if (strcmp(cmd, "led toggle") == 0) {
            led_external_toggle();
            Serial.println("-> External LED toggled.");
        }
        else {
            Serial.printf("-> Unknown command: '%s'\n", cmd);
        }
    }
}
