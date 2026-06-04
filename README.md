# Cooperative Task Scheduler for ESP8266

A lightweight cooperative task scheduler built from scratch for the ESP8266 (NodeMCU) in C++. The goal was to avoid pulling in FreeRTOS just to run a few periodic tasks — this handles it with a simple linked list and a timing check in the main loop.

---

# Why Cooperative Scheduling?

When building on the NodeMCU, managing multiple periodic tasks (polling sensors, blinking LEDs, reading serial commands) without `delay()` quickly becomes the core problem. There are three common approaches:

```
+---------------------------------------------------------------------------------+
|                                 1. BLOCKING DELAYS                              |
|  Loop:  [Read Sensor] -> (Delay 1000ms) -> [Blink LED] -> (Delay 500ms)         |
|  * The CPU sits doing nothing during delay. Miss serial input, miss sensor       |
|    edges, can't run anything else concurrently.                                 |
+---------------------------------------------------------------------------------+
                                         |
                                         v
+---------------------------------------------------------------------------------+
|                            2. PREEMPTIVE RTOS (FreeRTOS)                        |
|  Tasks interrupt each other by priority. Needs mutexes, semaphores, stacks.    |
|  * Works, but overkill for a NodeMCU project. Each task needs its own stack     |
|    allocation, and shared state becomes a race condition minefield.             |
+---------------------------------------------------------------------------------+
                                         |
                                         v
+---------------------------------------------------------------------------------+
|                       3. COOPERATIVE SCHEDULING (This Project)                  |
|  Tasks voluntarily yield. The main loop traverses a linked list of jobs and     |
|  fires any whose elapsed time has passed their interval.                        |
|  * No context switches, no stack-per-task overhead, no race conditions.         |
|    Runs fine even on an 80MHz ESP8266 with 80KB RAM.                            |
+---------------------------------------------------------------------------------+
```

### Comparison

| Metric | Blocking Delays (`delay`) | Preemptive RTOS (FreeRTOS) | Cooperative Scheduler (this) |
|---|---|---|---|
| **CPU Efficiency** | Low — wastes cycles sitting in delay loops | Moderate — context switch overhead | High — checks elapsed time and yields immediately |
| **RAM Footprint** | Extremely Low | High — separate stack per task | Extremely Low — single main stack shared by all tasks |
| **Simplicity** | Easy to write, hard to scale | Lots of boilerplate (mutexes, critical sections) | Straightforward — just register a function and an interval |
| **Race Conditions** | None (everything is sequential) | High risk without careful locking | None — single-threaded execution, tasks never interleave |
| **Max Concurrent Tasks** | 1, effectively | Limited by available RAM for stacks | As many as you want, CPU time permitting |

---

# Repository Layout

```text
├── platformio.ini         # PlatformIO project config (NodeMCU v2, 115200 baud)
├── src/
│   ├── main.cpp           # Clean boilerplate — start here
│   ├── my_scheduler.h     # The scheduler engine (header-only)
│   └── drivers/           # Hardware abstraction for LED, ADC, UART
└── examples/              # Four ready-to-run example programs
    ├── 01_blink.cpp
    ├── 02_dual_sensor_alarm.cpp
    ├── 03_one_shot_timers.cpp
    └── 04_cpu_profiling.cpp
```

---

# Scheduler Engine Architecture

Two classes: `Job` holds a task's interval, repeat count, callback pointer, and profiling data. `Scheduler` owns a linked list of `Job` pointers and walks it on every `loop()` call.

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

### Rollover-Safe Timing

`millis()` on a 32-bit unsigned counter wraps back to zero after ~49.7 days. The scheduler handles this transparently using unsigned subtraction:

```cpp
if (currentTime - current->lastExecutionTime >= current->intervalMs)
```

Because of how two's-complement unsigned arithmetic works, the subtraction produces the correct positive difference even across the wraparound boundary — no special-case needed.

For example: if `currentTime` just rolled over to `5` and `lastExecutionTime` is `4294967290` (near the 32-bit max), then `5 - 4294967290 = 11` in unsigned arithmetic, which is the actual elapsed time. The comparison still works correctly.

### Linked List Task Queue

Tasks are chained via a singly linked list rather than a fixed-size array. This keeps memory usage proportional to the number of registered tasks instead of pre-allocating a worst-case slot count. `core.add()` appends to the tail; `core.run()` walks the full list on every call.

One thing to keep in mind: `repeatCount` is decremented in-place on the `Job` object. If you want to restart a finite job (e.g., a one-shot timer), you need to set `repeatCount` back before calling `start()` again — see `examples/03_one_shot_timers.cpp` for how this works in practice.

---

# Examples

To run any example, copy its contents into `src/main.cpp` and flash to the board.

### 1. Blink — `examples/01_blink.cpp`
The "hello world" of this scheduler. Registers one job that toggles the onboard LED every 500ms. No `delay()`, just a timing check on each loop pass.

### 2. Dual-Sensor Alarm — `examples/02_dual_sensor_alarm.cpp`
A more realistic scenario — four concurrent tasks running at different rates:
- **IR proximity sensor** (GPIO4 / D2): polled every 100ms
- **Temperature sensor** (A0 / ADC0): read every 1500ms via `analogRead()`
- **Heartbeat LED** (onboard): blinks at 1s normally, switches to 150ms rapid flash when either sensor trips an alert
- **Serial CLI** (UART): parsed every 50ms for `stats` and `led toggle` commands

The LED blink rate change is done by directly modifying `heartbeatJob.intervalMs` inside the task — no extra state machine needed.

**Wiring:**
- IR sensor OUT → D2 (GPIO4), VCC → 3.3V, GND → GND
- External alarm LED → D1 (GPIO5) through a 330Ω resistor to GND
- Temperature sensor (LM35 or thermistor) → A0

### 3. One-Shot Software Timers — `examples/03_one_shot_timers.cpp`
Shows how `repeatCount` enables software timers. Typing `trigger 3000` over serial turns on the external LED and arms a job with `repeatCount = 1` and a 3000ms interval. When it fires, it turns the LED off and stops itself.

The key pattern: the one-shot job is registered to the scheduler at startup but left in a stopped state. It only activates when explicitly started with updated parameters.

### 4. CPU Profiling — `examples/04_cpu_profiling.cpp`
Intentionally includes a bad task that calls `delay(1000)` every 8 seconds to simulate blocking code. The other tasks (100ms fast blink, 1s logger) freeze during that window. Typing `stats` after a few block cycles shows the blocker's `Max(us)` column sitting at ~1,000,000µs while the other tasks show their normal sub-30µs numbers. Useful for understanding why you need to keep tasks short.

---

# Timing Diagnostics

Every task's execution time is measured using `micros()` brackets around the callback. `stats` over serial prints a live table:

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

If a task's `Max(us)` is dramatically higher than its `Last(us)`, it blocked the CPU at some point. The fix is to break the task into a state machine that does a small chunk of work per call instead of spinning in a loop.

---

# Getting Started

### 1. Build

```bash
pio run
```

Or if `pio` isn't in your PATH yet:

```bash
~/.platformio/penv/bin/platformio run
```

### 2. Upload to ESP8266

Close any open serial monitors first (they hold the port), then:

```bash
pio run --target upload
```

### 3. Open Serial Monitor

```bash
pio device monitor
```

Press `Ctrl+C` or `Ctrl+]` to exit before uploading again.
