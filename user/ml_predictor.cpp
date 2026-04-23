#include "ml_predictor.h"

#include <algorithm>
#include <cctype>

namespace {

static std::string toLower(std::string input) {
    std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return input;
}

static double clampScore(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

} // namespace

MLPrediction MLPredictor::predictProcess(const std::string& processName, size_t pageCount) const {
    std::string normalized = toLower(processName);
    double score = 0.1;
    std::string reason;

    if (normalized.find("browser") != std::string::npos) {
        score += 0.35;
        reason += "browser-like workload ";
    }
    if (normalized.find("electron") != std::string::npos) {
        score += 0.30;
        reason += "electron-like workload ";
    }
    if (normalized.find("vm") != std::string::npos || normalized.find("virtual") != std::string::npos) {
        score += 0.25;
        reason += "virtual-machine workload ";
    }
    if (pageCount > 64) {
        score += 0.15;
        reason += "large working set ";
    }
    if (pageCount < 16) {
        score -= 0.05;
        reason += "small working set ";
    }

    return {clampScore(score), reason};
}

MLPrediction MLPredictor::predictRegion(const std::string& regionName, size_t pageCount) const {
    std::string normalized = toLower(regionName);
    double score = 0.05;
    std::string reason;

    if (normalized.find("tab") != std::string::npos) {
        score += 0.40;
        reason += "tabbed content likely duplicates ";
    }
    if (normalized.find("cache") != std::string::npos) {
        score += 0.30;
        reason += "cache region likely repeated data ";
    }
    if (pageCount > 32) {
        score += 0.10;
        reason += "moderate region size ";
    }
    if (pageCount < 8) {
        score -= 0.05;
        reason += "tiny region ";
    }

    return {clampScore(score), reason};
}
