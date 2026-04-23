#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>

#define IOCTL_RAM_DEDUPE_REGISTER_PROCESS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_UNREGISTER_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_QUERY_STATS        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_LIST_PROCESSES     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_QUERY_HEALTH       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_DRAIN              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_INVOKE_COW_FAULT   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)

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
    BOOL CowInterceptionActive;
};

static bool expect(bool condition, const std::wstring& message) {
    if (!condition) {
        std::wcerr << L"Test failed: " << message << L"\n";
        return false;
    }
    return true;
}

static bool sendIoControl(HANDLE device, DWORD controlCode, PVOID inBuffer, DWORD inBufferSize, PVOID outBuffer, DWORD outBufferSize, DWORD& bytesReturned) {
    bytesReturned = 0;
    BOOL result = DeviceIoControl(device, controlCode, inBuffer, inBufferSize, outBuffer, outBufferSize, &bytesReturned, nullptr);
    if (!result) {
        std::wcerr << L"IOCTL 0x" << std::hex << controlCode << L" failed: " << std::dec << GetLastError() << L"\n";
        return false;
    }
    return true;
}

static bool queryStats(HANDLE device, DedupeDriverStats& stats) {
    DWORD bytesReturned = 0;
    if (!sendIoControl(device, IOCTL_RAM_DEDUPE_QUERY_STATS, nullptr, 0, &stats, sizeof(stats), bytesReturned)) {
        return false;
    }
    return bytesReturned >= sizeof(stats);
}

static bool queryHealth(HANDLE device, DedupeDriverHealth& health) {
    DWORD bytesReturned = 0;
    if (!sendIoControl(device, IOCTL_RAM_DEDUPE_QUERY_HEALTH, nullptr, 0, &health, sizeof(health), bytesReturned)) {
        return false;
    }
    return bytesReturned >= sizeof(health);
}

static bool testOptInPolicyBlocking(HANDLE device) {
    ULONG_PTR systemPid = 4;
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(device,
        IOCTL_RAM_DEDUPE_REGISTER_PROCESS,
        &systemPid,
        sizeof(systemPid),
        nullptr,
        0,
        &bytesReturned,
        nullptr);

    return expect(result == FALSE, L"System process opt-in should be rejected.");
}

static bool testOptInRoundTrip(HANDLE device) {
    ULONG_PTR currentPid = static_cast<ULONG_PTR>(GetCurrentProcessId());
    DWORD bytesReturned = 0;
    if (!sendIoControl(device, IOCTL_RAM_DEDUPE_REGISTER_PROCESS, &currentPid, sizeof(currentPid), nullptr, 0, bytesReturned)) {
        return expect(false, L"Failed to register current process for opt-in.");
    }

    std::vector<ULONG_PTR> listBuffer(64);
    if (!sendIoControl(device, IOCTL_RAM_DEDUPE_LIST_PROCESSES, nullptr, 0, listBuffer.data(), static_cast<DWORD>(listBuffer.size() * sizeof(ULONG_PTR)), bytesReturned)) {
        return expect(false, L"Process list query failed after registration.");
    }

    if (!expect(bytesReturned >= sizeof(ULONG_PTR), L"Process list query returned insufficient data.")) {
        return false;
    }

    ULONG_PTR count = listBuffer[0];
    bool foundCurrent = false;
    for (ULONG_PTR index = 0; index < count && index + 1 < listBuffer.size(); ++index) {
        if (listBuffer[index + 1] == currentPid) {
            foundCurrent = true;
            break;
        }
    }

    if (!expect(foundCurrent, L"Current process not found in opted-in list.")) {
        return false;
    }

    if (!sendIoControl(device, IOCTL_RAM_DEDUPE_DRAIN, nullptr, 0, nullptr, 0, bytesReturned)) {
        return expect(false, L"Dedupe drain failed.");
    }

    if (!sendIoControl(device, IOCTL_RAM_DEDUPE_UNREGISTER_PROCESS, &currentPid, sizeof(currentPid), nullptr, 0, bytesReturned)) {
        return expect(false, L"Failed to unregister current process.");
    }

    return true;
}

static bool testProcessListResizes(HANDLE device) {
    ULONG_PTR currentPid = static_cast<ULONG_PTR>(GetCurrentProcessId());
    DWORD bytesReturned = 0;
    if (!sendIoControl(device, IOCTL_RAM_DEDUPE_REGISTER_PROCESS, &currentPid, sizeof(currentPid), nullptr, 0, bytesReturned)) {
        return expect(false, L"Failed to register current process for process list resizing test.");
    }

    std::vector<ULONG_PTR> buffer(1);
    BOOL result = DeviceIoControl(
        device,
        IOCTL_RAM_DEDUPE_LIST_PROCESSES,
        nullptr,
        0,
        buffer.data(),
        static_cast<DWORD>(buffer.size() * sizeof(ULONG_PTR)),
        &bytesReturned,
        nullptr);

    if (!result) {
        DWORD err = GetLastError();
        if (err == ERROR_MORE_DATA || err == ERROR_INSUFFICIENT_BUFFER) {
            if (bytesReturned < sizeof(ULONG_PTR) * 2) {
                return expect(false, L"Process list resize response returned an invalid required size.");
            }

            size_t elementCount = static_cast<size_t>((bytesReturned + sizeof(ULONG_PTR) - 1) / sizeof(ULONG_PTR));
            buffer.resize(elementCount);
            if (!sendIoControl(device, IOCTL_RAM_DEDUPE_LIST_PROCESSES, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(ULONG_PTR)), bytesReturned)) {
                return expect(false, L"Retrying process list query failed after buffer resize.");
            }
        } else {
            return expect(false, L"Process list query failed when testing buffer growth.");
        }
    }

    if (!expect(bytesReturned >= sizeof(ULONG_PTR), L"Process list query returned insufficient data for the count field.")) {
        return false;
    }

    ULONG_PTR count = buffer[0];
    bool foundCurrent = false;
    for (ULONG_PTR index = 0; index < count && index + 1 < buffer.size(); ++index) {
        if (buffer[index + 1] == currentPid) {
            foundCurrent = true;
            break;
        }
    }

    if (!sendIoControl(device, IOCTL_RAM_DEDUPE_UNREGISTER_PROCESS, &currentPid, sizeof(currentPid), nullptr, 0, bytesReturned)) {
        return expect(false, L"Failed to unregister current process after process list resizing test.");
    }

    return expect(foundCurrent, L"Current process not found in process list after resizing.");
}

static bool testHealthAfterDrain(HANDLE device) {
    DedupeDriverHealth healthBefore{};
    if (!queryHealth(device, healthBefore)) {
        return expect(false, L"Health query failed before drain.");
    }

    DWORD bytesReturned = 0;
    if (!sendIoControl(device, IOCTL_RAM_DEDUPE_DRAIN, nullptr, 0, nullptr, 0, bytesReturned)) {
        return expect(false, L"Dedupe drain failed during health test.");
    }

    DedupeDriverHealth healthAfter{};
    if (!queryHealth(device, healthAfter)) {
        return expect(false, L"Health query failed after drain.");
    }

    return expect(healthAfter.CowProtectedPages == 0, L"COW-protected pages should be zero after drain.");
}

struct CowFaultInvoke {
    PVOID FaultingAddress;
    BOOLEAN WriteAccess;
};

static bool testCowCallbackStubInvocation(HANDLE device) {
    DWORD bytesReturned = 0;
    int localValue = 0;
    CowFaultInvoke invoke = { &localValue, TRUE };
    NTSTATUS callbackStatus = STATUS_UNSUCCESSFUL;

    if (!sendIoControl(device,
                       IOCTL_RAM_DEDUPE_INVOKE_COW_FAULT,
                       &invoke,
                       sizeof(invoke),
                       &callbackStatus,
                       sizeof(callbackStatus),
                       bytesReturned)) {
        return expect(false, L"Failed to invoke COW callback stub.");
    }

    if (!expect(bytesReturned == sizeof(callbackStatus), L"COW callback invocation returned unexpected output size.")) {
        return false;
    }

    return expect(callbackStatus == STATUS_NOT_SUPPORTED,
                  L"COW callback stub should return STATUS_NOT_SUPPORTED for a non-protected address.");
}

static bool testCowInterceptionReporting(HANDLE device) {
    DedupeDriverHealth health{};
    if (!queryHealth(device, health)) {
        return expect(false, L"Health query failed for COW interception reporting test.");
    }

    return expect(health.CowInterceptionActive == TRUE, L"COW interception should report active when callback registration stub is enabled.");
}

int wmain() {
    HANDLE device = CreateFileW(
        L"\\.\\RamDedupe",
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

    std::wcout << L"Running driver integration tests...\n";
    bool passed = true;

    passed &= testOptInPolicyBlocking(device);
    passed &= testProcessListResizes(device);
    passed &= testOptInRoundTrip(device);
    passed &= testHealthAfterDrain(device);
    passed &= testCowInterceptionReporting(device);
    passed &= testCowCallbackStubInvocation(device);

    DedupeDriverStats stats{};
    DedupeDriverHealth health{};
    passed &= expect(queryStats(device, stats), L"Stats query failed.");
    passed &= expect(queryHealth(device, health), L"Health query failed.");

    if (passed) {
        std::wcout << L"Driver integration tests passed.\n";
        CloseHandle(device);
        return 0;
    }

    std::wcerr << L"Driver integration tests failed.\n";
    CloseHandle(device);
    return 1;
}
