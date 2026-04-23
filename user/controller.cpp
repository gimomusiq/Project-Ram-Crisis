#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>

#define IOCTL_RAM_DEDUPE_REGISTER_PROCESS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_UNREGISTER_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_QUERY_STATS        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_LIST_PROCESSES     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_QUERY_HEALTH       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RAM_DEDUPE_DRAIN              CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

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

static void PrintUsage() {
    std::cout << "Usage:\n";
    std::cout << "  controller optin <pid>   - register a process for RAM deduplication opt-in\n";
    std::cout << "  controller optout <pid>  - remove a process from RAM dedupe opt-in\n";
    std::cout << "  controller stats         - query current dedupe driver stats\n";
    std::cout << "  controller list          - list opted-in process IDs\n";
    std::cout << "  controller health        - query current dedupe driver health\n";
    std::cout << "  controller drain         - drain shared dedupe state safely\n";
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    std::wstring command = argv[1];
    if (command != L"optin" && command != L"optout" && command != L"stats" && command != L"list" && command != L"health" && command != L"drain") {
        PrintUsage();
        return 1;
    }

    HANDLE device = CreateFileW(
        L"\\.\\RamDedupe",
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (device == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to open dedupe device (\\.\\RamDedupe). Error=" << GetLastError() << L"\n";
        return 1;
    }

    DWORD bytesReturned = 0;
    BOOL result = FALSE;

    if (command == L"optin" || command == L"optout") {
        if (argc != 3) {
            PrintUsage();
            CloseHandle(device);
            return 1;
        }

        DWORD pid = wcstoul(argv[2], nullptr, 10);
        if (pid == 0) {
            std::wcerr << L"Invalid PID: " << argv[2] << L"\n";
            CloseHandle(device);
            return 1;
        }

        ULONG_PTR processId = static_cast<ULONG_PTR>(pid);
        ULONG code = (command == L"optin") ? IOCTL_RAM_DEDUPE_REGISTER_PROCESS : IOCTL_RAM_DEDUPE_UNREGISTER_PROCESS;
        result = DeviceIoControl(
            device,
            code,
            &processId,
            sizeof(processId),
            nullptr,
            0,
            &bytesReturned,
            nullptr);

        if (!result) {
            std::wcerr << L"IOCTL failed with error " << GetLastError() << L"\n";
            CloseHandle(device);
            return 1;
        }

        if (command == L"optin") {
            std::wcout << L"Process " << pid << L" registered for RAM dedupe opt-in.\n";
        } else {
            std::wcout << L"Process " << pid << L" removed from RAM dedupe opt-in.\n";
        }
    } else if (command == L"stats") {
        DedupeDriverStats stats{};
        result = DeviceIoControl(
            device,
            IOCTL_RAM_DEDUPE_QUERY_STATS,
            nullptr,
            0,
            &stats,
            sizeof(stats),
            &bytesReturned,
            nullptr);

        if (!result) {
            std::wcerr << L"Statistics query failed with error " << GetLastError() << L"\n";
            CloseHandle(device);
            return 1;
        }

        std::wcout << L"Dedupe stats:\n";
        std::wcout << L"  Opted-in processes: " << stats.OptedInProcessCount << L"\n";
        std::wcout << L"  Unique page entries: " << stats.TotalPageEntries << L"\n";
        std::wcout << L"  Duplicate page entries: " << stats.DuplicatePageEntries << L"\n";
    } else if (command == L"health") {
        DedupeDriverHealth health{};
        result = DeviceIoControl(
            device,
            IOCTL_RAM_DEDUPE_QUERY_HEALTH,
            nullptr,
            0,
            &health,
            sizeof(health),
            &bytesReturned,
            nullptr);

        if (!result) {
            std::wcerr << L"Health query failed with error " << GetLastError() << L"\n";
            CloseHandle(device);
            return 1;
        }

        std::wcout << L"Dedupe health:\n";
        std::wcout << L"  Opted-in processes: " << health.OptedInProcessCount << L"\n";
        std::wcout << L"  Total page entries: " << health.TotalPageEntries << L"\n";
        std::wcout << L"  Duplicate page entries: " << health.DuplicatePageEntries << L"\n";
        std::wcout << L"  Shared page groups: " << health.SharedPageGroups << L"\n";
        std::wcout << L"  COW-protected pages: " << health.CowProtectedPages << L"\n";
        std::wcout << L"  COW interception: " << (health.CowInterceptionActive ? L"active" : L"inactive") << L"\n";
    } else if (command == L"drain") {
        result = DeviceIoControl(
            device,
            IOCTL_RAM_DEDUPE_DRAIN,
            nullptr,
            0,
            nullptr,
            0,
            &bytesReturned,
            nullptr);

        if (!result) {
            std::wcerr << L"Drain command failed with error " << GetLastError() << L"\n";
            CloseHandle(device);
            return 1;
        }

        std::wcout << L"Dedupe state drain completed.\n";
    } else {
        std::vector<ULONG_PTR> buffer(8);
        bool retry = true;
        bool success = false;

        while (retry) {
            result = DeviceIoControl(
                device,
                IOCTL_RAM_DEDUPE_LIST_PROCESSES,
                nullptr,
                0,
                buffer.data(),
                static_cast<DWORD>(buffer.size() * sizeof(ULONG_PTR)),
                &bytesReturned,
                nullptr);

            if (result) {
                success = true;
                retry = false;
            } else {
                DWORD err = GetLastError();
                if ((err == ERROR_MORE_DATA || err == ERROR_INSUFFICIENT_BUFFER) && bytesReturned > 0) {
                    size_t nextSize = static_cast<size_t>((bytesReturned + sizeof(ULONG_PTR) - 1) / sizeof(ULONG_PTR));
                    if (nextSize <= buffer.size()) {
                        nextSize = buffer.size() * 2;
                    }
                    buffer.resize(nextSize);
                    continue;
                }
                retry = false;
            }
        }

        if (!success) {
            std::wcerr << L"Process list query failed with error " << GetLastError() << L"\n";
            CloseHandle(device);
            return 1;
        }

        if (bytesReturned < sizeof(ULONG_PTR)) {
            std::wcerr << L"Process list query returned unexpected output size\n";
            CloseHandle(device);
            return 1;
        }

        ULONG_PTR count = buffer[0];
        size_t printable = static_cast<size_t>(count);
        if (printable > buffer.size() - 1) {
            printable = buffer.size() - 1;
            std::wcerr << L"Process list may be truncated by buffer size.\n";
        }

        std::wcout << L"Opted-in processes (" << count << L"):\n";
        for (size_t i = 0; i < printable; ++i) {
            std::wcout << L"  " << buffer[i + 1] << L"\n";
        }
    }

    CloseHandle(device);
    return 0;
}
