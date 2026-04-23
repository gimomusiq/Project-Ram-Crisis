#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

static constexpr size_t PageSize = 4096;

struct PageSignature {
    uint64_t hash;
    uint32_t fingerprint;
};

struct PageSignatureHash {
    size_t operator()(PageSignature sig) const noexcept;
};

struct PageSignatureEq {
    bool operator()(PageSignature a, PageSignature b) const noexcept;
};

struct Page {
    std::vector<uint8_t> data;
    size_t homeIndex;
    bool shared;

    explicit Page(std::vector<uint8_t> dataIn);
    bool isShared() const noexcept;
    void markShared(size_t representativeIndex) noexcept;
};

struct MemoryRegion {
    std::string name;
    bool optIn;
    std::vector<Page> pages;
};

class DedupeEngine {
public:
    struct Stats {
        size_t pagesScanned = 0;
        size_t duplicatePages = 0;
        size_t uniquePages = 0;
        size_t bytesSaved = 0;
        size_t collisionProbes = 0;
        size_t writesEmulated = 0;
        size_t writeCowEvents = 0;
    };

    DedupeEngine();
    ~DedupeEngine();

    void scanRegion(MemoryRegion& region);
    void writeToPage(MemoryRegion& region, size_t pageIndex, uint8_t value);
    Stats getStats() const;

private:
    PageSignature makeSignature(const Page& page) const;
    bool pagesAreEqual(const Page& a, const Page& b) const;
    void mergePage(MemoryRegion& region, size_t pageIndex, size_t representativeIndex);
    void clonePageForWrite(MemoryRegion& region, size_t pageIndex);

    Stats stats;
};
