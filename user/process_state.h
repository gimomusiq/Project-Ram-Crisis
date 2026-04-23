#pragma once

#include "dedupe_engine.h"

#include <cstdint>
#include <string>
#include <vector>

struct ProcessState {
    uint64_t pid;
    std::string name;
    bool optIn;
    std::vector<MemoryRegion> regions;
};
