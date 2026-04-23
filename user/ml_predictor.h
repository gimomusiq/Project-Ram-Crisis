#pragma once

#include <string>

struct MLPrediction {
    double score;
    std::string reason;
};

class MLPredictor {
public:
    MLPrediction predictProcess(const std::string& processName, size_t pageCount) const;
    MLPrediction predictRegion(const std::string& regionName, size_t pageCount) const;
};
