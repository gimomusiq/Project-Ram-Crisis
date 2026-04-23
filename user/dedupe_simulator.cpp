#include "dedupe_engine.h"
#include "ml_predictor.h"
#include "process_state.h"
#include "scan_scheduler.h"

#include <iostream>
#include <random>
#include <thread>

static std::vector<uint8_t> makePage(uint8_t baseValue, size_t seed) {
    std::vector<uint8_t> page(PageSize);
    std::mt19937_64 rng(seed);
    for (size_t i = 0; i < PageSize; ++i) {
        page[i] = static_cast<uint8_t>(baseValue + (rng() & 0xFF));
    }
    return page;
}

static MemoryRegion makeRegion(const std::string& name, bool optIn, size_t pages, uint8_t contentSeed) {
    MemoryRegion region;
    region.name = name;
    region.optIn = optIn;
    region.pages.reserve(pages);

    std::vector<std::vector<uint8_t>> samplePages;
    samplePages.reserve(8);
    for (size_t j = 0; j < 8; ++j) {
        samplePages.push_back(makePage(static_cast<uint8_t>(contentSeed + j * 3), contentSeed + 17 + j * 11));
    }

    for (size_t i = 0; i < pages; ++i) {
        if (!optIn) {
            region.pages.emplace_back(makePage(static_cast<uint8_t>(contentSeed + 4), contentSeed + 90 + i));
            continue;
        }

        if (i % 5 == 0) {
            region.pages.emplace_back(samplePages[0]);
        } else if (i % 5 == 1) {
            region.pages.emplace_back(samplePages[1]);
        } else if (i % 5 == 2) {
            region.pages.emplace_back(samplePages[(i / 5) % samplePages.size()]);
        } else if (i % 5 == 3) {
            region.pages.emplace_back(samplePages[(i / 3) % samplePages.size()]);
        } else {
            region.pages.emplace_back(makePage(static_cast<uint8_t>(contentSeed + 2), contentSeed + 50 + i));
        }
    }
    return region;
}

static size_t countSharedPages(const MemoryRegion& region) {
    size_t count = 0;
    for (const auto& page : region.pages) {
        if (page.isShared()) {
            ++count;
        }
    }
    return count;
}

int main() {
    std::cout << "RAM Dedupe Engine Prototype with Scheduler\n";
    std::cout << "--------------------------------------\n";

    ProcessState browserProcess;
    browserProcess.pid = 101;
    browserProcess.name = "BrowserRenderer";
    browserProcess.optIn = true;
    browserProcess.regions.push_back(makeRegion("BrowserTabs", true, 120, 0x20));
    browserProcess.regions.push_back(makeRegion("BrowserCache", true, 32, 0x21));

    ProcessState electronProcess;
    electronProcess.pid = 202;
    electronProcess.name = "ElectronApp";
    electronProcess.optIn = true;
    electronProcess.regions.push_back(makeRegion("ElectronMain", true, 60, 0x80));
    electronProcess.regions.push_back(makeRegion("ElectronCache", true, 24, 0x83));

    ProcessState systemProcess;
    systemProcess.pid = 1;
    systemProcess.name = "SystemKernel";
    systemProcess.optIn = false;
    systemProcess.regions.push_back(makeRegion("KernelPrivate", false, 40, 0xC0));

    DedupeEngine engine;
    MLPredictor predictor;
    ScanScheduler scheduler(engine, predictor);

    scheduler.addProcess(std::move(browserProcess));
    scheduler.addProcess(std::move(electronProcess));
    scheduler.addProcess(std::move(systemProcess));

    scheduler.start();
    scheduler.requestScan();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    scheduler.requestScan();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    scheduler.stop();

    auto snapshot = scheduler.snapshotProcesses();
    for (const auto& process : snapshot) {
        std::cout << "Process " << process.pid << " (" << process.name << ")";
        std::cout << " opt-in=" << (process.optIn ? "yes" : "no") << "\n";
        for (const auto& region : process.regions) {
            std::cout << "  " << region.name << "; pages=" << region.pages.size();
            std::cout << "; shared=" << countSharedPages(region) << "\n";
        }
    }

    auto stats = engine.getStats();
    std::cout << "\nAggregate stats:\n";
    std::cout << "  Pages scanned: " << stats.pagesScanned << "\n";
    std::cout << "  Unique pages: " << stats.uniquePages << "\n";
    std::cout << "  Duplicate pages: " << stats.duplicatePages << "\n";
    std::cout << "  Collision probes: " << stats.collisionProbes << "\n";
    std::cout << "  Copy-on-write events: " << stats.writeCowEvents << "\n";
    std::cout << "  Estimated RAM saved: " << stats.bytesSaved / 1024 << " KB\n";

    return 0;
}
