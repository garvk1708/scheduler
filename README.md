# Custom Cooperative Scheduler for ESP8266

A lightweight, fully custom cooperative task scheduler for the ESP8266 built entirely from scratch using modern embedded C++ principles.

This project demonstrates how to implement deterministic periodic task execution without using `delay()` or any external RTOS/library dependencies. It provides a clean and scalable architecture for writing responsive embedded firmware using cooperative multitasking, linked-list scheduling, modular drivers, and non-blocking execution.

The scheduler is designed around a simple principle:

> Tasks should execute periodically without blocking the CPU.

Instead of halting execution using `delay()`, the scheduler continuously checks elapsed time using `millis()` and executes tasks only when their scheduled interval expires.

---

# Features

- Cooperative multitasking scheduler
- Non-blocking firmware architecture
- Dynamic linked-list based task management
- Function-pointer driven job execution
- Modular driver abstraction
- UART command interface
- ADC sampling task
- Periodic LED control task
- Zero external scheduler dependencies
- PlatformIO + ESP8266 compatible
- Deterministic periodic execution model

---

# Why This Project Exists

Most beginner Arduino firmware looks like this:

```cpp
void loop() {
    blinkLED();
    delay(500);

    readSensor();
    delay(2000);
}
```

This architecture has major problems:

- CPU blocks during `delay()`
- UART input can be missed
- Tasks cannot run concurrently
- Timing becomes difficult to scale
- Firmware becomes unresponsive

This project replaces that design with a cooperative scheduler capable of running multiple independent periodic tasks simultaneously while keeping the CPU responsive.

---

# Project Structure

```text
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── my_scheduler.h
│   └── drivers/
│       ├── led.h
│       ├── led.cpp
│       ├── adc.h
│       ├── adc.cpp
│       ├── uart.h
│       └── uart.cpp
└── README.md
```

| File | Description |
|---|---|
| `main.cpp` | Example application using the scheduler |
| `my_scheduler.h` | Core scheduler implementation |
| `led.*` | Non-blocking LED driver |
| `adc.*` | ADC abstraction layer |
| `uart.*` | Non-blocking UART command parser |
| `platformio.ini` | PlatformIO configuration |

---

# Hardware Target

- ESP8266 NodeMCU
- Arduino Framework
- PlatformIO

## platformio.ini

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
```

---

# Scheduler Architecture

The scheduler is based on:

- periodic task polling
- linked-list traversal
- elapsed-time scheduling
- cooperative execution

The main execution loop is extremely small:

```cpp
void loop() {
    core.run();
}
```

The scheduler continuously scans all active jobs and executes them when their interval expires.

---

# Core Concepts

---

# 1. Job Objects

Each task in the system is represented using a `Job`.

Example:

```cpp
Job blinkJob(500, INF, &blinkLED);
```

This means:

| Parameter | Meaning |
|---|---|
| `500` | Execute every 500ms |
| `INF` | Run forever |
| `&blinkLED` | Function to execute |

---

# 2. Function Pointers

The scheduler stores task callbacks using function pointers:

```cpp
typedef void (*JobFunction)();
```

This allows the scheduler to dynamically execute arbitrary user-defined functions:

```cpp
current->actionFunction();
```

This is a common low-level embedded systems technique used in schedulers, interrupt systems, and callback frameworks.

---

# 3. Time-Based Scheduling

The scheduler avoids blocking execution using the ESP8266 system timer:

```cpp
millis()
```

Core scheduling equation:

```cpp
if (currentTime - current->lastExecutionTime >= current->intervalMs)
```

Mathematically:

Δt = t_current - t_last

Execute task if:

Δt ≥ T_interval

This creates deterministic periodic execution without halting the CPU.

---

# 4. Linked List Task Management

The scheduler dynamically chains jobs together using a singly linked list.

Internally:

```cpp
Job* nextJob;
```

Memory structure:

```text
firstJob
   |
   v
+------+    +------+    +------+
| Job1 | -> | Job2 | -> | Job3 | -> NULL
+------+    +------+    +------+
```

Advantages:

- dynamic task count
- no fixed scheduler array
- scalable architecture
- low memory overhead

---

# Full Scheduler Implementation

## Job Class

```cpp
class Job {
public:
    Job(unsigned long interval, int repeats, JobFunction action) {
        intervalMs = interval;
        repeatCount = repeats;
        actionFunction = action;
        isRunning = false;
        lastExecutionTime = 0;
        nextJob = nullptr;
    }

    void start() {
        isRunning = true;
        lastExecutionTime = millis();
    }

    void stop() {
        isRunning = false;
    }

    unsigned long intervalMs;
    int repeatCount;
    JobFunction actionFunction;
    bool isRunning;
    unsigned long lastExecutionTime;
    Job* nextJob;
};
```

---

## Scheduler Class

```cpp
class Scheduler {
public:
    Scheduler() {
        firstJob = nullptr;
    }

    void add(Job& newJob) {
        if (firstJob == nullptr) {
            firstJob = &newJob;
        } else {
            Job* current = firstJob;

            while (current->nextJob != nullptr) {
                current = current->nextJob;
            }

            current->nextJob = &newJob;
        }

        newJob.nextJob = nullptr;
    }

    void run() {
        unsigned long currentTime = millis();
        Job* current = firstJob;

        while (current != nullptr) {

            if (current->isRunning) {

                if (currentTime - current->lastExecutionTime
                    >= current->intervalMs) {

                    current->lastExecutionTime = currentTime;

                    if (current->actionFunction != nullptr) {
                        current->actionFunction();
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

private:
    Job* firstJob;
};
```

---

# How to Use the Scheduler

---

# Step 1: Include the Header

```cpp
#include "my_scheduler.h"
```

Create scheduler instance:

```cpp
Scheduler core;
```

---

# Step 2: Create Task Functions

Tasks should be:

- short
- non-blocking
- fast-returning

Bad:

```cpp
void task() {
    delay(5000);
}
```

Good:

```cpp
void blinkLED() {
    led_toggle();
}
```

---

# Step 3: Create Jobs

```cpp
Job blinkJob(500, INF, &blinkLED);
Job adcJob(2000, INF, &readAnalog);
Job serialJob(50, INF, &handleSerial);
```

---

# Step 4: Register Jobs

```cpp
core.add(blinkJob);
core.add(adcJob);
core.add(serialJob);
```

---

# Step 5: Start Jobs

```cpp
blinkJob.start();
adcJob.start();
serialJob.start();
```

---

# Step 6: Run Scheduler

```cpp
void loop() {
    core.run();
}
```

---

# Example Application

## main.cpp

```cpp
#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "my_scheduler.h"

#include "drivers/led.h"
#include "drivers/adc.h"
#include "drivers/uart.h"

Scheduler core;

void blinkLED();
void readAnalog();
void handleSerial();

Job blinkJob(500, INF, &blinkLED);
Job adcJob(2000, INF, &readAnalog);
Job serialJob(50, INF, &handleSerial);

void setup() {

    uart_init(115200);
    led_init();
    adc_init();

    core.add(blinkJob);
    core.add(adcJob);
    core.add(serialJob);

    blinkJob.start();
    adcJob.start();
    serialJob.start();

    wifi_set_sleep_type(LIGHT_SLEEP_T);
}

void loop() {
    core.run();
}

void blinkLED() {
    led_toggle();
}

void readAnalog() {
    uint16_t val = adc_read_nonblocking();
    Serial.println(val);
}

void handleSerial() {

    char cmd[64];

    if (uart_get_command_nonblocking(cmd, sizeof(cmd))) {

        if (strcmp(cmd, "led on") == 0) {
            blinkJob.stop();
            led_set(1);
        }

        else if (strcmp(cmd, "led off") == 0) {
            blinkJob.stop();
            led_set(0);
        }

        else if (strcmp(cmd, "led toggle") == 0) {
            led_toggle();
        }

        else if (strcmp(cmd, "blink on") == 0) {
            blinkJob.start();
        }
    }
}
```

---

# UART Command Interface

The firmware includes a non-blocking serial command parser.

Supported commands:

| Command | Action |
|---|---|
| `led on` | Turn LED on |
| `led off` | Turn LED off |
| `led toggle` | Toggle LED |
| `blink on` | Resume blinking task |

Example handler:

```cpp
if (strcmp(cmd, "led on") == 0) {
    blinkJob.stop();
    led_set(1);
}
```

The UART system never blocks scheduler execution.

---

# Driver Design

The project uses modular hardware abstraction layers.

Instead of:

```cpp
digitalWrite(...)
```

directly everywhere, drivers expose clean APIs:

```cpp
led_toggle();
led_set();
adc_read_nonblocking();
uart_get_command_nonblocking();
```

Benefits:

- modular architecture
- reusable drivers
- cleaner firmware
- hardware abstraction
- improved maintainability

---

# LED Driver

The onboard ESP8266 LED is active-low.

Meaning:

```cpp
digitalWrite(LOW);
```

turns the LED ON.

Driver handles this internally.

---

# ADC System

The ADC driver wraps:

```cpp
analogRead(A0)
```

ESP8266 ADC characteristics:

| Property | Value |
|---|---|
| Resolution | 10-bit |
| Range | 0–1023 |

ADC conversion relationship:

```text
ADC = (Vin / Vref) × 1023
```

---

# Runtime Behavior

Example execution timeline:

| Time | Event |
|---|---|
| 50ms | UART poll |
| 500ms | LED toggle |
| 1000ms | LED toggle |
| 2000ms | ADC sample |
| 2050ms | UART poll |

All tasks execute cooperatively without blocking each other.

---

# Advantages of Cooperative Scheduling

- simple architecture
- low memory usage
- deterministic execution
- no context-switch overhead
- easy debugging
- ideal for small MCUs

---

# Current Limitations

This is not a full RTOS.

Current limitations include:

- no task priorities
- no preemption
- no mutex/semaphore system
- O(n) task traversal
- no interrupt-driven scheduling
- long tasks block the scheduler

---

# Possible Future Improvements

Potential extensions:

- priority scheduling
- task deletion
- dynamic task creation
- software timers
- interrupt-triggered tasks
- EDF scheduling
- rate-monotonic scheduling
- watchdog integration
- ISR-safe queues
- event-driven architecture
- lightweight RTOS transition

---

# Concepts Demonstrated

This project demonstrates practical understanding of:

- embedded firmware architecture
- cooperative multitasking
- linked lists
- function pointers
- non-blocking programming
- periodic scheduling
- modular driver development
- UART command parsing
- hardware abstraction
- deterministic embedded systems

---

# Build & Upload

Using PlatformIO:

```bash
pio run
pio run --target upload
pio device monitor
```

---

# Example Serial Output

```text
512
514
509
520
```

---

# Educational Value

This project resembles simplified versions of:

- cyclic executives
- event-loop firmware
- cooperative RTOS kernels
- embedded runtime systems
- lightweight task schedulers

It serves as an excellent stepping stone toward:

- FreeRTOS
- Zephyr
- embedded Linux
- asynchronous runtimes
- real-time systems design

---

# License

MIT License

Feel free to modify, extend, and use this project for educational or embedded systems development purposes.
