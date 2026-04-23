#pragma once

#include "dedupe_engine.h"
#include "ml_predictor.h"
#include "process_state.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

class ScanScheduler {
public:
    explicit ScanScheduler(DedupeEngine& engine, MLPredictor& predictor);
    ~ScanScheduler();

    void addProcess(ProcessState process);
    void start();
    void stop();
    void requestScan();
    std::vector<ProcessState> snapshotProcesses() const;

private:
    void workerLoop();
    void scanNextCandidate();

    DedupeEngine& engine;
    MLPredictor& predictor;

    mutable std::mutex mutex;
    std::condition_variable condition;
    std::vector<ProcessState> processes;
    std::atomic<bool> running;
    std::atomic<bool> scanRequested;
    std::thread worker;
    std::string scoreDetail;
};
