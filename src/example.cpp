#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/adc.h"
#include "drivers/uart.h"

// Define GPIO pin for IR Sensor (D2 on NodeMCU / GPIO4)
#define IR_SENSOR_PIN 4

// Define Scheduler instance
Scheduler core;

// Declare Task Functions
void blinkHeartbeat();
void readIRSensor();
void readTemperatureSensor();
void handleUARTShell();

// Create Job Objects (Interval in ms, Repeat Count, Callback Function)
Job heartbeatJob(1000, INF, &blinkHeartbeat);       // Blinks internal LED as status indicator
Job irSensorJob(100, INF, &readIRSensor);            // Polls IR sensor every 100ms
Job tempSensorJob(1500, INF, &readTemperatureSensor); // Reads temperature sensor every 1.5s
Job uartShellJob(50, INF, &handleUARTShell);          // Runs CLI command parser every 50ms

// Shared variables (State representation)
volatile bool irTriggered = false;
volatile bool tempHigh = false;
uint16_t latestTempRaw = 0;

// Threshold for Temperature Alarm (analog range 0 to 1023)
const uint16_t TEMP_ALERT_THRESHOLD = 700;

void setup() {
    // Initialize Drivers
    custom_uart_init(115200);
    led_init();
    adc_init();

    // Initialize IR Sensor Pin (input mode)
    pinMode(IR_SENSOR_PIN, INPUT);

    // Register Jobs to Scheduler Queue
    core.add(heartbeatJob);
    core.add(irSensorJob);
    core.add(tempSensorJob);
    core.add(uartShellJob);

    // Start all jobs
    heartbeatJob.start();
    irSensorJob.start();
    tempSensorJob.start();
    uartShellJob.start();

    // Print Boot Info
    Serial.println("\n========================================================");
    Serial.println("   Cooperative Scheduler - Dual Sensor & LED Example");
    Serial.println("========================================================");
    Serial.println("Hardware configuration:");
    Serial.printf("  - Internal LED : GPIO%d (D4) - Onboard Heartbeat\n", LED_PIN);
    Serial.printf("  - External LED : GPIO%d (D1) - Alarm Output\n", EXT_LED_PIN);
    Serial.printf("  - IR Sensor    : GPIO%d (D2) - Proximity Input\n", IR_SENSOR_PIN);
    Serial.printf("  - Temp Sensor  : A0 (ADC0)   - Analog Thermal Input\n");
    Serial.println("--------------------------------------------------------");
    Serial.println("Supported Console Commands:");
    Serial.println("  'stats'        - View real-time scheduler task profiling");
    Serial.println("  'led toggle'   - Manually toggle the external LED");
    Serial.println("========================================================\n");

    // Disable WiFi radio to ensure stable analog ADC measurements and save power
    WiFi.mode(WIFI_OFF);
    wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
    // Keep polling tasks inside the scheduler list
    core.run();
}

// ----------------------------------------------------
// Task Implementations
// ----------------------------------------------------

// Task 1: Blink Internal LED (Heartbeat & Status Signal)
// Usecase: Blinks slowly (1000ms) under normal conditions. 
// If an alert is active (IR or Temperature high), blinks rapidly (150ms) to indicate warnings.
void blinkHeartbeat() {
    led_internal_toggle();
    
    // Dynamically scale blink frequency based on system state
    if (irTriggered || tempHigh) {
        heartbeatJob.intervalMs = 150; // Fast panic flash
    } else {
        heartbeatJob.intervalMs = 1000; // Slow calm pulse
    }
}

// Task 2: Poll IR Proximity Sensor (Digital Read)
// Usecase: Reads digital output from IR obstacle sensor module.
// If proximity is detected (typically active-LOW on cheap IR sensor modules, but let's read logic level):
// We trigger the External LED immediately as a physical alarm response.
void readIRSensor() {
    // Read state from IR sensor pin. (Most modules output LOW when obstacle is detected)
    bool currentVal = (digitalRead(IR_SENSOR_PIN) == LOW);
    
    if (currentVal != irTriggered) {
        irTriggered = currentVal;
        if (irTriggered) {
            Serial.println("[EVENT] IR Sensor: Obstacle / Proximity DETECTED!");
            led_external_set(1); // Set Alarm LED ON
        } else {
            Serial.println("[EVENT] IR Sensor: Cleared.");
            // Turn off alarm LED only if temperature is also safe
            if (!tempHigh) {
                led_external_set(0);
            }
        }
    }
}

// Task 3: Read Temperature Sensor (Analog Read A0)
// Usecase: Measures voltage level from LM35 or Thermistor.
// If reading goes above threshold, sets alert flag and switches external alarm LED on.
void readTemperatureSensor() {
    latestTempRaw = adc_read_nonblocking();
    
    if (latestTempRaw > TEMP_ALERT_THRESHOLD) {
        if (!tempHigh) {
            tempHigh = true;
            Serial.printf("[ALERT] Temp High! Raw ADC: %d (Limit: %d)\n", latestTempRaw, TEMP_ALERT_THRESHOLD);
            led_external_set(1); // Activate external alert LED
        }
    } else {
        if (tempHigh) {
            tempHigh = false;
            Serial.printf("[SAFE] Temp Normal. Raw ADC: %d\n", latestTempRaw);
            // Deactivate alarm LED if IR is also clear
            if (!irTriggered) {
                led_external_set(0);
            }
        }
    }
}

// Task 4: CLI Console Command Parser
void handleUARTShell() {
    char cmd[64];
    
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strcmp(cmd, "stats") == 0) {
            Serial.println("\n--- Scheduler Diagnostics & Timing Stats ---");
            Serial.printf("%-15s | %-6s | %-12s | %-12s | %-12s | %-12s | %-12s\n", 
                          "Task Name", "State", "Interval(ms)", "Executions", "Last(us)", "Max(us)", "Avg(us)");
            Serial.println("---------------------------------------------------------------------------------------------");
            
            Job* current = core.getFirstJob();
            while (current != nullptr) {
                const char* name = "Unknown";
                if (current->actionFunction == &blinkHeartbeat) name = "BlinkHeartbeat";
                else if (current->actionFunction == &readIRSensor) name = "ReadIRSensor";
                else if (current->actionFunction == &readTemperatureSensor) name = "ReadTempSensor";
                else if (current->actionFunction == &handleUARTShell) name = "UARTShell";

                unsigned long avgRunTime = 0;
                if (current->executionCount > 0) {
                    avgRunTime = current->totalRunTimeUs / current->executionCount;
                }

                Serial.printf("%-15s | %-6s | %-12lu | %-12lu | %-12lu | %-12lu | %-12lu\n",
                              name,
                              current->isRunning ? "RUN" : "STOP",
                              current->intervalMs,
                              current->executionCount,
                              current->lastRunTimeUs,
                              current->maxRunTimeUs,
                              avgRunTime);
                
                current = current->nextJob;
            }
            Serial.println("---------------------------------------------------------------------------------------------\n");
        }
        else if (strcmp(cmd, "led toggle") == 0) {
            led_external_toggle();
            Serial.println("-> External LED manually toggled.");
        }
        else {
            Serial.printf("-> Unknown command: '%s'\n", cmd);
        }
    }
}
