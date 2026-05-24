#ifndef MY_SCHEDULER_H
#define MY_SCHEDULER_H

#include <Arduino.h>

#define INF -1

typedef void (*JobFunction)();

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
                if (currentTime - current->lastExecutionTime >= current->intervalMs) {
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

#endif
