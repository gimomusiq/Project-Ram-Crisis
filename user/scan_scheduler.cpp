#include "scan_scheduler.h"

#include <algorithm>
#include <chrono>
#include <sstream>

ScanScheduler::ScanScheduler(DedupeEngine& engine, MLPredictor& predictor)
    : engine(engine), predictor(predictor), running(false), scanRequested(false) {}

ScanScheduler::~ScanScheduler() {
    stop();
}

void ScanScheduler::addProcess(ProcessState process) {
    std::lock_guard<std::mutex> lock(mutex);
    processes.emplace_back(std::move(process));
}

void ScanScheduler::start() {
    if (running.exchange(true)) {
        return;
    }
    worker = std::thread(&ScanScheduler::workerLoop, this);
}

void ScanScheduler::stop() {
    if (!running.exchange(false)) {
        return;
    }
    scanRequested = true;
    condition.notify_all();
    if (worker.joinable()) {
        worker.join();
    }
}

void ScanScheduler::requestScan() {
    scanRequested = true;
    condition.notify_one();
}

std::vector<ProcessState> ScanScheduler::snapshotProcesses() const {
    std::lock_guard<std::mutex> lock(mutex);
    return processes;
}

void ScanScheduler::workerLoop() {
    while (running) {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait_for(lock, std::chrono::milliseconds(500), [this] {
            return scanRequested.load() || !running.load();
        });

        if (!running) {
            break;
        }

        scanRequested = false;
        if (processes.empty()) {
            continue;
        }

        scanNextCandidate();
    }
}

void ScanScheduler::scanNextCandidate() {
    double bestScore = -1.0;
    size_t bestProcessIndex = SIZE_MAX;
    size_t bestRegionIndex = SIZE_MAX;
    std::string bestDetail;

    {
        std::lock_guard<std::mutex> lock(mutex);
        for (size_t processIndex = 0; processIndex < processes.size(); ++processIndex) {
            auto& process = processes[processIndex];
            if (!process.optIn) {
                continue;
            }

            auto processPrediction = predictor.predictProcess(process.name, process.regions.size());
            for (size_t regionIndex = 0; regionIndex < process.regions.size(); ++regionIndex) {
                auto& region = process.regions[regionIndex];
                auto regionPrediction = predictor.predictRegion(region.name, region.pages.size());
                double score = (processPrediction.score + regionPrediction.score) / 2.0;
                if (score > bestScore) {
                    bestScore = score;
                    bestProcessIndex = processIndex;
                    bestRegionIndex = regionIndex;
                    std::ostringstream detail;
                    detail << processPrediction.reason << regionPrediction.reason;
                    bestDetail = detail.str();
                }
            }
        }
    }

    if (bestProcessIndex == SIZE_MAX || bestRegionIndex == SIZE_MAX) {
        return;
    }

    MemoryRegion targetRegion;
    {
        std::lock_guard<std::mutex> lock(mutex);
        targetRegion = processes[bestProcessIndex].regions[bestRegionIndex];
    }

    engine.scanRegion(targetRegion);

    {
        std::lock_guard<std::mutex> lock(mutex);
        processes[bestProcessIndex].regions[bestRegionIndex] = std::move(targetRegion);
    }

    scoreDetail = std::move(bestDetail);
}
