#include <Windows.h>
#include <iostream>
#include <chrono>

#define IOCTL_RAM_DEDUPE_QUERY_STATS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_QUERY_HEALTH  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct DedupeDriverStats {
    ULONG OptedInProcessCount;
    ULONG TotalPageEntries;
    ULONG DuplicatePageEntries;
};

struct DedupeDriverHealth {
    ULONG OptedInProcessCount;
    ULONG TotalPageEntries;
    ULONG DuplicatePageEntries;
    ULONG SharedPageGroups;
    ULONG CowProtectedPages;
    ULONG CowPrivateCopies;
    BOOL CowInterceptionActive;
};

int wmain(int argc, wchar_t* argv[]) {
    const wchar_t* devicePath = L"\\.\RamDedupe";
    if (argc >= 2) {
        devicePath = argv[1];
    }

    HANDLE device = CreateFileW(
        devicePath,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (device == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open dedupe device: " << GetLastError() << L"\n";
        return 1;
    }

    const int iterations = 50;
    std::cout << "Driver benchmark: " << iterations << " iterations" << std::endl;

    DedupeDriverStats stats{};
    DedupeDriverHealth health{};
    DWORD bytesReturned = 0;

    auto runQuery = [&](DWORD controlCode, PVOID outBuffer, DWORD outBufferSize) -> bool {
        return DeviceIoControl(device, controlCode, nullptr, 0, outBuffer, outBufferSize, &bytesReturned, nullptr) != 0 && bytesReturned >= outBufferSize;
    };

    auto benchmarkQuery = [&](DWORD controlCode, PVOID outBuffer, DWORD outBufferSize, const char* label) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            if (!runQuery(controlCode, outBuffer, outBufferSize)) {
                std::cerr << label << " query failed on iteration " << i << "\n";
                return false;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto durationMs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        std::cout << label << " average latency: " << (durationMs / iterations) << " ms" << std::endl;
        return true;
    };

    if (!benchmarkQuery(IOCTL_RAM_DEDUPE_QUERY_STATS, &stats, sizeof(stats), "Stats")) {
        CloseHandle(device);
        return 1;
    }

    if (!benchmarkQuery(IOCTL_RAM_DEDUPE_QUERY_HEALTH, &health, sizeof(health), "Health")) {
        CloseHandle(device);
        return 1;
    }

    std::cout << "Driver stats snapshot:" << std::endl;
    std::cout << "  Opted-in processes: " << stats.OptedInProcessCount << std::endl;
    std::cout << "  Total page entries: " << stats.TotalPageEntries << std::endl;
    std::cout << "  Duplicate page entries: " << stats.DuplicatePageEntries << std::endl;

    std::cout << "Driver health snapshot:" << std::endl;
    std::cout << "  Shared page groups: " << health.SharedPageGroups << std::endl;
    std::cout << "  COW-protected pages: " << health.CowProtectedPages << std::endl;
    std::cout << "  COW private copies: " << health.CowPrivateCopies << std::endl;
    std::cout << "  COW interception active: " << (health.CowInterceptionActive ? "yes" : "no") << std::endl;

    CloseHandle(device);
    return 0;
}
