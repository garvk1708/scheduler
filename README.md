# Custom Cooperative Scheduler for ESP8266

This project implements a custom, lightweight, object-oriented cooperative task scheduler specifically designed for the ESP8266. It provides a clean API for running deterministic, periodic tasks without blocking the main execution loop, and it is built entirely from scratch with zero external library dependencies.

## Project Structure

```text
├── platformio.ini              # PlatformIO build configuration
├── src/
│   ├── main.cpp                # Example usage of the scheduler
│   ├── my_scheduler.h          # The core custom scheduler implementation
│   └── drivers/
│       ├── led.h / led.cpp     # Basic non-blocking LED driver
│       ├── adc.h / adc.cpp     # Basic analog read driver
│       └── uart.h / uart.cpp   # Basic interrupt-safe serial command driver
└── README.md
```

## How to Use It (Step-by-Step)

Using this scheduler is designed to be extremely simple. You do not need to manage timers or `delay()` calls yourself. 

### Step 1: Include the Header and Create a Runner
Include `my_scheduler.h` at the top of your file and create a global `Scheduler` object. This object will manage all your tasks.
```cpp
#include "my_scheduler.h"
Scheduler core;
```

### Step 2: Define Your Action Functions
Write standard void functions that perform the actions you want to happen periodically. **Crucially, these functions should not contain any `delay()` calls or blocking `while()` loops.**
```cpp
void blinkLED() {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}
void readSensor() {
    Serial.println(analogRead(A0));
}
```

### Step 3: Instantiate Your Jobs
Create `Job` objects for each function. The constructor takes three arguments:
1. **Interval (ms)**: How often the job should run.
2. **Repeats**: How many times it should run. Pass `INF` (-1) to run forever.
3. **Action**: A pointer to the function you wrote in Step 2.
```cpp
Job blinkJob(500, INF, &blinkLED); // Runs every 500ms forever
Job sensorJob(2000, 10, &readSensor); // Runs every 2000ms, exactly 10 times, then stops
```

### Step 4: Add and Start Jobs in `setup()`
In your Arduino `setup()` function, add the jobs to the runner queue and call `start()` on them to begin their internal timers.
```cpp
void setup() {
    core.add(blinkJob);
    core.add(sensorJob);
    
    blinkJob.start();
    sensorJob.start();
}
```

### Step 5: Execute in `loop()`
Finally, simply call the `run()` method on your `JobRunner` inside the main `loop()`. The runner handles everything else.
```cpp
void loop() {
    core.run();
}
```

---

## How It Works (In-Depth)

The scheduler avoids the blocking nature of `delay()` by continuously tracking elapsed time using the ESP8266's internal `millis()` counter. Here is a deep dive into the architecture:

### 1. The Linked List Data Structure
Inside `my_scheduler.h`, the `Scheduler` class does not use a fixed-size array. Instead, it uses a **Linked List**. 
- Every `Job` object contains a pointer called `nextJob`. 
- When you call `core.add(newJob)`, the `Scheduler` traverses its list of jobs starting from `firstJob`, and attaches your new job to the end of the chain. This allows you to add as many tasks as your RAM can handle dynamically.

### 2. Time Tracking (`millis()`)
When you call `job.start()`, the job records the exact current system time into its `lastExecutionTime` variable. 

Inside the main `loop()`, when `core.run()` is called, it iterates through the entire linked list of jobs. For each job that is currently marked as `isRunning`, it performs a simple subtraction:
```cpp
if (currentTime - current->lastExecutionTime >= current->intervalMs) { ... }
```
If the elapsed time is greater than or equal to the job's defined `intervalMs`, the scheduler knows it is time to execute that job. 

### 3. Execution and Repeat Management
When a job's time has come:
1. It immediately updates its `lastExecutionTime` to the current time so the next interval can begin accurately.
2. It executes the provided `actionFunction`.
3. It checks the `repeatCount`. If the count is greater than zero (meaning it is not set to `INF`), it subtracts 1. Once the count hits zero, the job automatically calls `stop()` on itself, removing it from the active execution pool without needing to be deleted from memory.

By structuring the code this way, multiple tasks can interleave gracefully. While the `blinkLED` task is waiting for its 500ms timer to expire, the CPU is completely free to check the UART buffer or read the ADC sensor.
