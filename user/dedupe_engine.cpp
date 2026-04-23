#include "dedupe_engine.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <unordered_map>

namespace {

uint64_t fastHash64(const uint8_t* data, size_t length) {
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    hash ^= (hash >> 33);
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= (hash >> 33);
    hash *= 0xc4ceb9fe1a85ec53ULL;
    hash ^= (hash >> 33);
    return hash;
}

uint32_t computeFingerprint(const uint8_t* data, size_t length) {
    uint32_t acc = 0x811C9DC5U;
    constexpr size_t steps = 8;
    const size_t stride = length / steps;
    for (size_t i = 0; i < steps; ++i) {
        size_t idx = i * stride;
        uint32_t value = static_cast<uint32_t>(data[idx]) << 16 |
                         static_cast<uint32_t>(data[idx + 1]) << 8 |
                         static_cast<uint32_t>(data[idx + 2]);
        acc ^= value;
        acc *= 16777619U;
    }
    return acc ^ static_cast<uint32_t>(length);
}

} // namespace

size_t PageSignatureHash::operator()(PageSignature sig) const noexcept {
    return static_cast<size_t>(sig.hash ^ (sig.hash >> 32) ^ sig.fingerprint);
}

bool PageSignatureEq::operator()(PageSignature a, PageSignature b) const noexcept {
    return a.hash == b.hash && a.fingerprint == b.fingerprint;
}

Page::Page(std::vector<uint8_t> dataIn)
    : data(std::move(dataIn)), homeIndex(SIZE_MAX), shared(false) {}

bool Page::isShared() const noexcept {
    return shared;
}

void Page::markShared(size_t representativeIndex) noexcept {
    homeIndex = representativeIndex;
    shared = true;
}

DedupeEngine::DedupeEngine() = default;

DedupeEngine::~DedupeEngine() = default;

DedupeEngine::Stats DedupeEngine::getStats() const {
    return stats;
}

PageSignature DedupeEngine::makeSignature(const Page& page) const {
    const uint8_t* data = page.data.data();
    return {fastHash64(data, page.data.size()), computeFingerprint(data, page.data.size())};
}

bool DedupeEngine::pagesAreEqual(const Page& a, const Page& b) const {
    return a.data.size() == b.data.size() &&
           std::memcmp(a.data.data(), b.data.data(), a.data.size()) == 0;
}

void DedupeEngine::mergePage(MemoryRegion& region, size_t pageIndex, size_t representativeIndex) {
    auto& page = region.pages[pageIndex];
    if (page.homeIndex == representativeIndex) {
        return;
    }
    page.markShared(representativeIndex);
    stats.duplicatePages += 1;
    stats.bytesSaved += PageSize;
}

void DedupeEngine::clonePageForWrite(MemoryRegion& region, size_t pageIndex) {
    auto& page = region.pages[pageIndex];
    if (!page.isShared()) {
        return;
    }

    stats.writeCowEvents += 1;
    Page newPage(page.data);
    newPage.homeIndex = SIZE_MAX;
    newPage.shared = false;
    region.pages[pageIndex] = std::move(newPage);
}

void DedupeEngine::scanRegion(MemoryRegion& region) {
    if (!region.optIn) {
        return;
    }

    std::unordered_map<PageSignature, std::vector<size_t>, PageSignatureHash, PageSignatureEq> signatureMap;
    signatureMap.reserve(region.pages.size());

    for (size_t index = 0; index < region.pages.size(); ++index) {
        auto& page = region.pages[index];
        if (page.isShared()) {
            continue;
        }

        stats.pagesScanned += 1;
        PageSignature signature = makeSignature(page);

        auto& collisionList = signatureMap[signature];
        stats.collisionProbes += collisionList.size();
        bool duplicateFound = false;
        for (size_t representativeIndex : collisionList) {
            if (pagesAreEqual(region.pages[representativeIndex], page)) {
                mergePage(region, index, representativeIndex);
                duplicateFound = true;
                break;
            }
        }

        if (!duplicateFound) {
            collisionList.push_back(index);
            stats.uniquePages += 1;
        }
    }
}

void DedupeEngine::writeToPage(MemoryRegion& region, size_t pageIndex, uint8_t value) {
    if (pageIndex >= region.pages.size()) {
        return;
    }
    stats.writesEmulated += 1;
    auto& page = region.pages[pageIndex];

    if (page.isShared()) {
        clonePageForWrite(region, pageIndex);
    }

    if (page.data.empty()) {
        page.data.resize(PageSize);
    }
    page.data[0] = value;
}
