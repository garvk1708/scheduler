# Custom Cooperative Scheduler for ESP8266

A lightweight, fully custom cooperative task scheduler for the ESP8266 built entirely from scratch using modern embedded C++ principles. 

This project demonstrates how to implement deterministic periodic task execution without using `delay()` or any external RTOS/library dependencies. It provides a clean and scalable architecture for writing responsive embedded firmware using cooperative multitasking, linked-list scheduling, modular drivers, and non-blocking execution.

---

# Features

- **Cooperative Multitasking Scheduler**: Runs multiple periodic tasks concurrently without preemption overhead.
- **Hardware-Level Non-Blocking Architecture**: Exposes hardware abstractions without utilizing CPU-blocking code.
- **Dynamic Task Management**: Singly linked-list based structure allowing runtime modifications.
- **Real-Time Task Profiling**: Built-in microsecond-level timing diagnostics for execution time tracking.
- **System Alarm Use Case**: Interactive example utilizing an internal LED, external LED, and analog sensor.
- **UART Command Shell**: Simple built-in interactive CLI parser to toggle pins and view stats.

---

# Hardware Mapping

This project is tailored for the **ESP8266 NodeMCU V2**, but runs on any ESP8266 module:

| Device/Component | Pin / Interface | Active State | Purpose |
|---|---|---|---|
| **Internal Onboard LED** | `GPIO2` / `D4` | **Active-Low** | Safe heartbeat (slow blink) / Alarm alert (rapid flash) |
| **External Alarm LED** | `GPIO5` / `D1` | **Active-High** | Secondary indicator toggled via CLI command |
| **Analog Sensor** (LDR/Thermistor) | `ADC0` / `A0` | `0` to `1023` (10-bit) | Inputs light/temperature readings for threshold analysis |
| **UART Shell Interface** | Serial / USB | `115200 Baud` | Control LEDs and query scheduler telemetry |

---

# Getting Started

Follow these steps to build, flash, and test the project:

### 1. Requirements & Prerequisites
Ensure you have the PlatformIO Core CLI installed (which we set up in the `~/.platformio/penv` directory) or have VSCode with the PlatformIO extension.

### 2. Connect Your ESP8266
Plug your ESP8266 into your computer's USB port. It should be detected as a serial device (typically `/dev/ttyUSB0` on Linux, `/dev/tty.usbserial` on macOS, or `COMx` on Windows).

### 3. Compile the Firmware
Open your terminal in the project directory and run the compilation script:
```bash
# Compile using the PlatformIO command line
~/.platformio/penv/bin/platformio run
```

### 4. Upload to ESP8266
Flash the compiled binary to the connected board:
```bash
~/.platformio/penv/bin/platformio run --target upload
```

### 5. Monitor and Control
Open the serial interactive terminal at `115200 baud` rate:
```bash
~/.platformio/penv/bin/platformio device monitor
```
Once connected, try entering the commands:
- `led on`
- `led off`
- `led toggle`
- `stats`

---

# Practical Usecase: Smart Ambient Monitor & Alarm

The included [main.cpp](file:///home/garv/Desktop/scheduler/src/main.cpp) implements a **Smart Ambient Monitor and Alarm System**. 

```
                                  +-------------------+
  Analog Sensor (LDR/Temp) ------>|    A0 (ADC Read)  |
                                  +---------+---------+
                                            |
                                            v  (Every 1000ms)
                                  +-------------------+
                                  | Threshold Check   |
                                  |    (Limit: 600)   |
                                  +---------+---------+
                                            |
                         +------------------+------------------+
                         |                                     |
                (If Value > 600)                      (If Value <= 600)
                         v                                     v
             +-----------------------+             +-----------------------+
             |   Rapid Warning Blink |             |   Slow Heartbeat Blink|
             |       (Interval = 100ms) |             |      (Interval = 1000ms) |
             +-----------------------+             +-----------------------+
```

### How the Usecase Works:
1. **Heartbeat / Status Loop**: The onboard **Internal LED** blinks slowly at a `1000ms` interval under normal conditions, signaling the system is functioning correctly.
2. **Periodic Sensor Sampling**: Every `1000ms`, the **Sensor Task** reads the analog input on pin `A0` (connected to an LDR light-sensor, potentiometer, or thermistor).
3. **Dynamic Scheduling Reconfiguration**:
   - If the sensor value exceeds `600` (representing high heat or light), the Sensor Task **re-schedules** the internal LED blinking task to execute at a rapid `100ms` warning rate.
   - It also prints an alert message: `[ALERT] Sensor Exceeded Threshold! Value: XXX`.
   - Once the sensor value drops below the threshold, the blinking task returns to the slow `1000ms` heartbeat interval.
4. **Independent External Control**: The **External LED** connected to D1 remains fully controllable by the user via UART commands without interfering with the timing of the sensor task or alarm loop.

---

# Command Shell CLI

Through the UART serial monitor, you can issue commands directly to the ESP8266:

| Command | Action |
|---|---|
| `led on` | Turn ON the External LED (`GPIO5` / `D1`) |
| `led off` | Turn OFF the External LED (`GPIO5` / `D1`) |
| `led toggle` | Toggle the state of the External LED |
| `stats` | Output diagnostic statistics for the scheduler tasks |

### Interactive Diagnostics Output (`stats`)
Running `stats` prints a diagnostic report of the cooperative scheduler's performance:

```text
--- Scheduler Diagnostics & Timing Stats ---
Job Name     | State  | Interval(ms) | Executions   | Last(us)     | Max(us)      | Avg(us)     
-----------------------------------------------------------------------------------------
BlinkInternal| RUN    | 1000         | 42           | 2            | 12           | 3           
SampleSensor | RUN    | 1000         | 42           | 112          | 145          | 116         
UARTCommands | RUN    | 50           | 840          | 1            | 8            | 1           
-----------------------------------------------------------------------------------------
```

- **Job Name**: The custom label for each task.
- **State**: Whether the job is actively being polled (`RUN`) or suspended (`STOP`).
- **Interval(ms)**: The configured execution interval.
- **Executions**: Total times the job callback function has run since boot.
- **Last(us)**: The execution duration of the job's last run in microseconds.
- **Max(us)**: The longest execution time recorded (peak CPU usage for this task).
- **Avg(us)**: The average execution duration of the task. Keep this as low as possible to maintain a responsive cooperative scheduling environment!

---

# Architecture & Code Implementation

## 1. Job and Scheduler Implementation (`src/my_scheduler.h`)

The scheduler maintains an active list of job nodes linked sequentially. Inside the execution block, it uses the standard delta check against `millis()` to handle CPU rollover safely. It profiles runtime in microseconds (`micros()`) around callback execution:

```cpp
void run() {
    unsigned long currentTime = millis();
    Job* current = firstJob;
    
    while (current != nullptr) {
        if (current->isRunning) {
            if (currentTime - current->lastExecutionTime >= current->intervalMs) {
                current->lastExecutionTime = currentTime;
                
                if (current->actionFunction != nullptr) {
                    unsigned long startTimeUs = micros();
                    current->actionFunction();
                    unsigned long elapsedUs = micros() - startTimeUs;

                    // Update timing statistics
                    current->executionCount++;
                    current->lastRunTimeUs = elapsedUs;
                    current->totalRunTimeUs += elapsedUs;
                    if (elapsedUs > current->maxRunTimeUs) {
                        current->maxRunTimeUs = elapsedUs;
                    }
                }
                
                if (current->repeatCount > 0) {
                    current->repeatCount--;
                    if (current->repeatCount == 0) {
                        current->stop();
                    }
                }
            }
        }
        current = current->nextJob;
    }
}
```

## 2. Example Application (`src/main.cpp`)

The system initialization registers the tasks, sets pin configurations, and transitions to the scheduler runner loop:

```cpp
#include <Arduino.h>
#include "my_scheduler.h"
#include "drivers/led.h"
#include "drivers/adc.h"
#include "drivers/uart.h"

Scheduler core;

void blinkInternalLED();
void sampleSensor();
void handleUARTCommands();

Job blinkJob(1000, INF, &blinkInternalLED);
Job adcJob(1000, INF, &sampleSensor);
Job serialJob(50, INF, &handleUARTCommands);

void setup() {
    custom_uart_init(115200);
    led_init();
    adc_init();

    core.add(blinkJob);
    core.add(adcJob);
    core.add(serialJob);

    blinkJob.start();
    adcJob.start();
    serialJob.start();
}

void loop() {
    core.run(); // Keeps executing all registered non-blocking tasks
}
```

---

# Advantages of Cooperative Scheduling

1. **Deterministic Execution**: Tasks are called at reliable timing intervals when standard coding structures are designed properly.
2. **Zero Context-Switch Overhead**: Avoids complex register stacking or stack allocations required by preemptive RTOS cores.
3. **No Synchronization Primitive Requirements**: Because tasks do not interrupt one another, there is no chance of race conditions, deadlocks, or need for Semaphores/Mutexes.
4. **Lightweight Footprint**: Memory usage is minimized to simple `Job` node instances chained inside RAM.
