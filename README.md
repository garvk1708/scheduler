# Cooperative Task Scheduler for ESP8266

A custom, low-overhead cooperative task scheduler built from scratch for the ESP8266 (NodeMCU) in embedded C++. This project demonstrates how to run concurrent periodic operations without blocking the CPU or using resource-heavy real-time operating systems (RTOS).

---

# Why Cooperative Task Scheduling?

In embedded systems development, managing multiple periodic tasks (e.g., polling sensors, blinking status lights, parsing serial commands) is a core challenge. Developers typically rely on one of three patterns:

```
+---------------------------------------------------------------------------------+
|                                 1. BLOCKING DELAYS                              |
|  Loop:  [Read Sensor] -> (Delay 1000ms) -> [Blink LED] -> (Delay 500ms)         |
|  * Disadvantage: The CPU blocks completely during delay, losing data & command  |
|                  inputs. Concurrent operations are impossible.                  |
+---------------------------------------------------------------------------------+
                                         |
                                         v
+---------------------------------------------------------------------------------+
|                            2. PREEMPTIVE RTOS (FreeRTOS)                        |
|  Tasks interrupt each other based on priority. Needs mutexes/semaphores.       |
|  * Disadvantage: Significant RAM overhead, stack allocation requirements, context|
|                  switching lag, and risk of deadlocks/race conditions.          |
+---------------------------------------------------------------------------------+
                                         |
                                         v
+---------------------------------------------------------------------------------+
|                       3. COOPERATIVE SCHEDULING (This Project)                  |
|  Tasks voluntarily yield control. Traverses a linked list checking elapsed      |
|  time on each loop iteration. Runs tasks when scheduled interval expires.       |
|  * Advantage: Zero context-switching lag, extremely lightweight (runs on small  |
|               microcontrollers), zero race conditions (single thread execution).|
+---------------------------------------------------------------------------------+
```

### Architectural Comparison

| Metric | Blocking Delays (`delay`) | Preemptive RTOS (FreeRTOS) | Cooperative Scheduler (Our design) |
|---|---|---|---|
| **CPU Efficiency** | Low (wastes cycles in delay loops) | Moderate (context switch overhead) | **High** (checks elapsed time and yields) |
| **RAM Footprint** | Extremely Low | High (requires separate stack per task) | **Extremely Low** (uses single main stack) |
| **Simplicity** | High (simple but non-functional) | Low (requires mutexes, critical sections) | **High** (straightforward C++ execution) |
| **Race Conditions**| None (sequential execution) | High risk (requires thread safety) | **None** (sequential non-blocking execution) |
| **Max Concurrent Tasks** | 1 (effectively) | Limited by RAM stacks (e.g., 5-10) | **Unlimited** (limited only by CPU bandwidth) |

---

# Scheduler Engine Architecture

The scheduler is built around two primary abstractions: a `Job` (representing a task) and the `Scheduler` (handling task traversal).

```
                      Scheduler Object
                             |
                             v
                     +---------------+
                     |   firstJob    |
                     +-------+-------+
                             |
                             v
+----------------+   +----------------+   +----------------+
|      Job 1     |   |      Job 2     |   |      Job 3     |
| - intervalMs   |   | - intervalMs   |   | - intervalMs   |
| - repeatCount  |-->| - repeatCount  |-->| - repeatCount  |--> NULL
| - actionFunc   |   | - actionFunc   |   | - actionFunc   |
| - nextJob      |   | - nextJob      |   | - nextJob      |
+----------------+   +----------------+   +----------------+
```

### 1. Rollover-Safe Timing Equation
Inside embedded microcontrollers, system clock counters like `millis()` will eventually overflow and wrap around to `0` (for 32-bit unsigned integers, this occurs every **49.7 days**). The scheduler handles this rollover seamlessly using unsigned subtraction:

$$\Delta t = t_{\text{current}} - t_{\text{last}}$$

```cpp
if (currentTime - current->lastExecutionTime >= current->intervalMs)
```
Due to two's-complement arithmetic, if $t_{\text{current}}$ rolls over (e.g., $5$) and $t_{\text{last}}$ is near the maximum limit (e.g., $2^{32} - 10$), the subtraction wraps around to the correct positive difference ($15$), guaranteeing timing stability indefinitely.

### 2. Linked List Task Queue
Instead of allocating a fixed-size array which restricts flexibility, tasks are chained using a singly linked list. When a job is registered via `core.add(newJob)`, it is appended to the list tail in $O(N)$ time. Traversal in the execution loop is a linear $O(N)$ sweep, which completes in microseconds.

---

# Step-by-Step Interfacing Example

The project workspace contains a fully structured example illustrating how to read two sensors and actuate two outputs concurrently without blocking.

### Hardware Interface Configuration

```
                         ESP8266 (NodeMCU)
                         +---------------+
                         |   GPIO2/D4    |---> [Internal Onboard LED] (Active-Low)
                         |               |
                         |   GPIO5/D1    |---> [External Alarm LED]   (Active-High)
                         |               |
   [IR Sensor] --------->|   GPIO4/D2    |
 (Digital Proximity)     |               |
                         |     A0        |<--- [Temp Sensor] (LM35/Thermistor)
                         +---------------+
```

The example code is placed in [example.cpp](file:///home/garv/Desktop/scheduler/src/example.cpp). To use it, simply copy its contents into `main.cpp` or change the source filter configuration in your PlatformIO build settings.

### Fully Documented Example Code
Below is the C++ implementation showing how the cooperative tasks interact via shared states:

```cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/adc.h"
#include "drivers/uart.h"

#define IR_SENSOR_PIN 4
#define TEMP_ALERT_THRESHOLD 700

Scheduler core;

void blinkHeartbeat();
void readIRSensor();
void readTemperatureSensor();
void handleUARTShell();

// Instantiate Job objects (Interval in ms, Repeat Count, Function pointer)
Job heartbeatJob(1000, INF, &blinkHeartbeat);       // Blinks internal LED
Job irSensorJob(100, INF, &readIRSensor);            // Polls digital IR sensor
Job tempSensorJob(1500, INF, &readTemperatureSensor); // Reads analog temperature
Job uartShellJob(50, INF, &handleUARTShell);          // Command parser

volatile bool irTriggered = false;
volatile bool tempHigh = false;
uint16_t latestTempRaw = 0;

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

    // Disable WiFi to conserve power and reduce ADC noise
    WiFi.mode(WIFI_OFF);
    wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
    core.run(); // Checks time deltas and runs pending jobs
}

// TASK 1: System Heartbeat Status Blinker
void blinkHeartbeat() {
    led_internal_toggle();
    
    // Scale blinking frequency based on alarm states
    if (irTriggered || tempHigh) {
        heartbeatJob.intervalMs = 150; // Rapid alert flash
    } else {
        heartbeatJob.intervalMs = 1000; // Slow calm heartbeat pulse
    }
}

// TASK 2: IR Proximity Sensor Polling
void readIRSensor() {
    // Read state from digital IR Sensor. LOW means proximity detected
    bool currentVal = (digitalRead(IR_SENSOR_PIN) == LOW);
    
    if (currentVal != irTriggered) {
        irTriggered = currentVal;
        if (irTriggered) {
            Serial.println("[ALERT] IR Sensor: Obstacle Detected!");
            led_external_set(1); // Set Alarm LED ON
        } else {
            Serial.println("[STATUS] IR Sensor: Cleared.");
            if (!tempHigh) led_external_set(0); // Set Alarm LED OFF
        }
    }
}

// TASK 3: Analog Temperature Sensor Monitor
void readTemperatureSensor() {
    latestTempRaw = adc_read_nonblocking();
    
    if (latestTempRaw > TEMP_ALERT_THRESHOLD) {
        if (!tempHigh) {
            tempHigh = true;
            Serial.printf("[ALERT] Temperature Threshold Exceeded! Raw: %d\n", latestTempRaw);
            led_external_set(1);
        }
    } else {
        if (tempHigh) {
            tempHigh = false;
            Serial.printf("[STATUS] Temperature returned to normal. Raw: %d\n", latestTempRaw);
            if (!irTriggered) led_external_set(0);
        }
    }
}

// TASK 4: UART Command Interpreter Shell
void handleUARTShell() {
    char cmd[64];
    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {
        if (strcmp(cmd, "stats") == 0) {
            // Stats printing code...
        }
    }
}
```

---

# Timing Diagnostics and Profiling

A critical issue in cooperative scheduling is that a single blocking or poorly designed task (e.g. executing `delay(100)`) halts the entire scheduler, delaying other jobs.

To monitor this, the scheduler implements microsecond-level timing diagnostics around callback executions using `micros()`. Running the `stats` command via the Serial interface prints a task execution dashboard:

```text
--- Scheduler Diagnostics & Timing Stats ---
Task Name       | State  | Interval(ms) | Executions   | Last(us)     | Max(us)      | Avg(us)     
---------------------------------------------------------------------------------------------
BlinkHeartbeat  | RUN    | 1000         | 120          | 2            | 14           | 3           
ReadIRSensor    | RUN    | 100          | 1200         | 4            | 28           | 5           
ReadTempSensor  | RUN    | 1500         | 80           | 112          | 168          | 115         
UARTShell       | RUN    | 50           | 2400         | 1            | 12           | 2           
---------------------------------------------------------------------------------------------
```

- **Last(us)**: The execution duration of the job's last run in microseconds.
- **Max(us)**: The longest execution time recorded. Helpful for identifying peak latency spikes.
- **Avg(us)**: The average execution duration of the task. If any task reports values in the milliseconds range ($>1000\text{ }\mu\text{s}$), it indicates a blocking operation that should be refactored to prevent latency propagation across other tasks.

---

# Getting Started Guide

### 1. Compile the Custom Application
Open your console shell in the directory and compile the program:
```bash
# Builds main.cpp (configured inside platformio.ini)
~/.platformio/penv/bin/platformio run
```

### 2. Upload Firmware
Upload the compiled binaries to the ESP8266 board:
```bash
~/.platformio/penv/bin/platformio run --target upload
```

### 3. Open Serial Shell Monitor
Interact with the command shell via PlatformIO's device monitor:
```bash
~/.platformio/penv/bin/platformio device monitor
```
Type `stats` into the prompt to review task performance statistics.
