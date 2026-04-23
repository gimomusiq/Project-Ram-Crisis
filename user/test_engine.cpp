#include "dedupe_engine.h"

#include <algorithm>
#include <iostream>
#include <string>

static std::vector<uint8_t> makeConstantPage(uint8_t value) {
    std::vector<uint8_t> page(PageSize, value);
    return page;
}

static bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Test failed: " << message << std::endl;
        return false;
    }
    return true;
}

static bool testSimpleDedup() {
    MemoryRegion region;
    region.name = "TestDuplicate";
    region.optIn = true;
    region.pages.emplace_back(makeConstantPage(0x10));
    region.pages.emplace_back(makeConstantPage(0x10));
    region.pages.emplace_back(makeConstantPage(0x20));

    DedupeEngine engine;
    engine.scanRegion(region);
    auto stats = engine.getStats();

    return expect(stats.pagesScanned == 3, "pagesScanned should be 3") &&
           expect(stats.duplicatePages == 1, "duplicatePages should be 1") &&
           expect(stats.uniquePages == 2, "uniquePages should be 2") &&
           expect(count_if(region.pages.begin(), region.pages.end(), [](auto const& p) { return p.isShared(); }) == 1, "one page should be shared");
}

static bool testCowOnWrite() {
    MemoryRegion region;
    region.name = "TestCow";
    region.optIn = true;
    region.pages.emplace_back(makeConstantPage(0x30));
    region.pages.emplace_back(makeConstantPage(0x30));

    DedupeEngine engine;
    engine.scanRegion(region);
    engine.writeToPage(region, 1, 0x55);
    auto stats = engine.getStats();

    return expect(stats.duplicatePages == 1, "duplicatePages should be 1") &&
           expect(stats.writeCowEvents == 1, "writeCowEvents should be 1") &&
           expect(region.pages[1].isShared() == false, "page should not remain shared after write");
}

static bool testOptOutRegion() {
    MemoryRegion region;
    region.name = "TestOptOut";
    region.optIn = false;
    region.pages.emplace_back(makeConstantPage(0x40));
    region.pages.emplace_back(makeConstantPage(0x40));

    DedupeEngine engine;
    engine.scanRegion(region);
    auto stats = engine.getStats();

    return expect(stats.pagesScanned == 0, "pagesScanned should be 0") &&
           expect(stats.duplicatePages == 0, "duplicatePages should be 0");
}

int main() {
    std::cout << "Running dedupe engine tests...\n";
    bool passed = true;
    passed &= testSimpleDedup();
    passed &= testCowOnWrite();
    passed &= testOptOutRegion();
    if (!passed) {
        std::cout << "Some tests failed.\n";
        return 1;
    }
    std::cout << "All tests passed.\n";
    return 0;
}
