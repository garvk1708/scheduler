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

# Repository Layout

```text
├── platformio.ini         # PlatformIO project configuration
├── src/
│   ├── main.cpp           # Barebones boilerplate ready for custom tasks
│   ├── my_scheduler.h     # Core timing execution scheduler engine
│   └── drivers/           # Hardware abstraction layers (LED, ADC, UART)
└── examples/              # Preconfigured application reference files
    ├── 01_blink.cpp
    ├── 02_dual_sensor_alarm.cpp
    ├── 03_one_shot_timers.cpp
    └── 04_cpu_profiling.cpp
```

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
Instead of allocating a fixed-size array which restricts flexibility, tasks are chained using a singly linked list. When a job is registered via `core.add(newJob)`, it is appended to the list tail in $O(N)$ time. Traversal in the execution loop is a linear $O(N)$ sweep.

---

# Preconfigured Examples Index

To explore different cooperative multitasking configurations, you can copy the contents of any file in [examples/](file:///home/garv/Desktop/scheduler/examples/) directly into [src/main.cpp](file:///home/garv/Desktop/scheduler/src/main.cpp) and compile/upload it to your ESP8266.

### [1. Blink Example](file:///home/garv/Desktop/scheduler/examples/01_blink.cpp)
- **Concept**: A simple non-blocking status blinker.
- **Goal**: Demonstrates registering a basic callback function to toggle the internal onboard LED every 500ms without utilizing `delay()`.

### [2. Dual-Sensor Alarm Example](file:///home/garv/Desktop/scheduler/examples/02_dual_sensor_alarm.cpp)
- **Concept**: A physical safety monitoring alarm loop.
- **Hardware setup**:
  - **IR Proximity Sensor**: Digital sensor reading on GPIO4 (D2).
  - **Temperature Sensor**: Analog input on A0.
  - **Internal LED**: Onboard heartbeat.
  - **External LED**: GPIO5 (D1) alarm output.
- **Goal**: Illustrates dynamic task rescheduling. The internal LED blinks slowly (1s) under safe states but switches to a rapid panic flash (150ms) if the IR sensor detects an obstacle or the temperature goes above a defined threshold. The external LED acts as a physical alarm flag.

### [3. One-Shot Software Timers](file:///home/garv/Desktop/scheduler/examples/03_one_shot_timers.cpp)
- **Concept**: Dynamic task lifecycle management.
- **Goal**: Shows how to run a task a finite number of times (one-shot). When a user types `trigger 3000` via the CLI monitor, it switches ON the external LED and schedules a timer job with a `repeatCount = 1` to execute in 3000ms. Once the timer finishes, it turns OFF the LED and self-suspends.

### [4. CPU Timing Profiling](file:///home/garv/Desktop/scheduler/examples/04_cpu_profiling.cpp)
- **Concept**: Detecting blocking execution.
- **Goal**: Simulates a "badly designed" blocking function that blocks the CPU for 1000ms (`delay(1000)`) every 8 seconds. This demonstrates task execution jitter: when the blocker task runs, other tasks freeze. Querying `stats` will immediately isolate the blocker by showing a massive `Max(us)` execution time value.

---

# Timing Diagnostics and Profiling

A critical rule of cooperative scheduling is that tasks must be **short and non-blocking**. If a task halts the CPU, other tasks are starved.

To monitor CPU usage, the scheduler integrates microsecond-level timing tracking around callback executions using `micros()`. Typing `stats` in the serial prompt displays a timing breakdown:

```text
--- Scheduler Timing Diagnostics ---
Task Name       | Interval(ms) | Executions   | Last(us)     | Max(us)      | Avg(us)     
--------------------------------------------------------------------------------------
FastTask(100ms) | 100          | 1200         | 4            | 28           | 5           
LoggerTask(1s)  | 1000         | 120          | 2            | 14           | 3           
Blocker(8s)     | 8000         | 15           | 1000142      | 1000214      | 1000150     
UARTShell(50ms) | 50           | 2400         | 1            | 12           | 2           
--------------------------------------------------------------------------------------
```
If you detect any task reporting high execution times (e.g., `Blocker(8s)` with $1,000,150\text{ }\mu\text{s}$), it indicates CPU blocking that should be refactored into shorter state-machine steps.

---

# Getting Started Guide

### 1. Build the Boilerplate Project
To compile the clean scheduler template in [src/main.cpp](file:///home/garv/Desktop/scheduler/src/main.cpp):
```bash
~/.platformio/penv/bin/platformio run
```

### 2. Upload to ESP8266
Ensure that your board is connected and any active Serial Monitors are **closed** (to prevent port lock collisions), then run:
```bash
~/.platformio/penv/bin/platformio run --target upload
```

### 3. Open Serial Shell Monitor
To open the console shell monitor to communicate with the board:
```bash
~/.platformio/penv/bin/platformio device monitor
```
Press `Ctrl+C` or `Ctrl+]` to close the serial monitor before uploading subsequent code.
