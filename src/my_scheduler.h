#ifndef MY_SCHEDULER_H
#define MY_SCHEDULER_H

#include <Arduino.h>

// Pass INF as repeatCount to keep a job running indefinitely
#define INF -1

typedef void (*JobFunction)();

class Job {
public:
    Job(unsigned long interval, int repeats, JobFunction action) 
        : intervalMs(interval),
          repeatCount(repeats),
          actionFunction(action),
          isRunning(false),
          lastExecutionTime(0),
          nextJob(nullptr),
          executionCount(0),
          maxRunTimeUs(0),
          totalRunTimeUs(0),
          lastRunTimeUs(0) {}

    void start() {
        isRunning = true;
        lastExecutionTime = millis();
    }

    void stop() {
        isRunning = false;
    }

    unsigned long intervalMs;
    // Note: repeatCount is decremented in-place each execution. To restart a
    // finite job (e.g. a one-shot timer), reset repeatCount before calling start().
    int repeatCount;
    JobFunction actionFunction;
    bool isRunning;
    unsigned long lastExecutionTime;
    Job* nextJob;

    // Per-task execution timing — populated by the scheduler on every run
    unsigned long executionCount;
    unsigned long maxRunTimeUs;
    unsigned long totalRunTimeUs;
    unsigned long lastRunTimeUs;
};

class Scheduler {
public:
    Scheduler() : firstJob(nullptr) {}

    // Appends a job to the end of the queue. Don't add the same job twice —
    // that will corrupt the nextJob pointer and cause an infinite loop in run().
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
                // Unsigned subtraction handles millis() rollover correctly —
                // the difference wraps to the right value even across the 32-bit boundary
                if (currentTime - current->lastExecutionTime >= current->intervalMs) {
                    current->lastExecutionTime = currentTime;
                    
                    if (current->actionFunction != nullptr) {
                        unsigned long startTimeUs = micros();
                        current->actionFunction();
                        unsigned long elapsedUs = micros() - startTimeUs;

                        current->executionCount++;
                        current->lastRunTimeUs = elapsedUs;
                        current->totalRunTimeUs += elapsedUs;
                        if (elapsedUs > current->maxRunTimeUs) {
                            current->maxRunTimeUs = elapsedUs;
                        }
                    }
                    
                    // Decrement the repeat counter; stop the job once exhausted
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

    Job* getFirstJob() const {
        return firstJob;
    }

private:
    Job* firstJob;
};

#endif
