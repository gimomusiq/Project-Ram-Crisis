#include "dedupe_engine.h"
#include "process_state.h"

#include <iostream>
#include <random>
#include <string>

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
    samplePages.reserve(4);
    for (size_t j = 0; j < 4; ++j) {
        samplePages.push_back(makePage(static_cast<uint8_t>(contentSeed + j * 8), contentSeed + 17 + j * 11));
    }

    for (size_t i = 0; i < pages; ++i) {
        if (optIn && i % 4 != 3) {
            region.pages.emplace_back(samplePages[i % samplePages.size()]);
        } else {
            region.pages.emplace_back(makePage(static_cast<uint8_t>(contentSeed + 3), contentSeed + 90 + i));
        }
    }

    return region;
}

int main() {
    std::cout << "RAM Dedupe Benchmark\n";
    std::cout << "-------------------\n";

    MemoryRegion browser = makeRegion("BrowserTabs", true, 120, 0x10);
    MemoryRegion electron = makeRegion("ElectronApp", true, 72, 0x40);
    MemoryRegion service = makeRegion("ServiceCache", true, 48, 0x70);
    MemoryRegion kernel = makeRegion("KernelMemory", false, 32, 0xA0);

    DedupeEngine engine;
    engine.scanRegion(browser);
    engine.scanRegion(electron);
    engine.scanRegion(service);
    engine.scanRegion(kernel);

    auto stats = engine.getStats();
    size_t totalPages = browser.pages.size() + electron.pages.size() + service.pages.size() + kernel.pages.size();

    std::cout << "Total pages modeled: " << totalPages << "\n";
    std::cout << "Pages scanned: " << stats.pagesScanned << "\n";
    std::cout << "Unique pages: " << stats.uniquePages << "\n";
    std::cout << "Duplicate pages: " << stats.duplicatePages << "\n";
    std::cout << "Estimated RAM saved: " << stats.bytesSaved / 1024 << " KB\n";
    std::cout << "Copy-on-write events: " << stats.writeCowEvents << "\n";
    std::cout << "Simulated savings: " << (stats.bytesSaved / static_cast<double>(totalPages * PageSize) * 100.0) << "%\n";

    return 0;
}
