#pragma once

#include <ntddk.h>

#define IOCTL_RAM_DEDUPE_REGISTER_PROCESS    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_RAM_DEDUPE_UNREGISTER_PROCESS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_RAM_DEDUPE_QUERY_STATS         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_RAM_DEDUPE_LIST_PROCESSES      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_RAM_DEDUPE_QUERY_HEALTH        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_RAM_DEDUPE_DRAIN               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_RAM_DEDUPE_INVOKE_COW_FAULT    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define RAM_DEDUPE_POOL_TAG 'pDeR'

struct CowFaultInvoke {
    PVOID FaultingAddress;
    BOOLEAN WriteAccess;
};

struct PageSignature {
    UINT64 Hash;
    UINT32 Fingerprint;
};

struct ProcessOptInEntry {
    HANDLE ProcessId;
};

struct SharedPageGroup {
    PageSignature Signature;
    ULONG ReferenceCount;
    ULONG PageFrameCount;
    PFN_NUMBER PageFrameNumbers[64];
};

struct CowProtectedPage {
    PFN_NUMBER PageFrameNumber;
};

struct PageEntry {
    PageSignature Signature;
    ULONG ReferenceCount;
    ULONG PageFrameCount;
    PFN_NUMBER PageFrameNumbers[64];
    BOOLEAN Shared;
};

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
    BOOLEAN CowInterceptionActive;
};

struct DedupeContext {
    RTL_GENERIC_TABLE ProcessTable;
    RTL_GENERIC_TABLE PageTable;
    RTL_GENERIC_TABLE SharedGroupTable;
    RTL_GENERIC_TABLE CowProtectedTable;
};

extern DedupeContext gDedupeContext;

NTSTATUS InitializeDedupeDriver();
VOID ShutdownDedupeDriver();
NTSTATUS RegisterOptInProcess(HANDLE ProcessId);
NTSTATUS UnregisterOptInProcess(HANDLE ProcessId);
BOOLEAN IsProcessOptedIn(HANDLE ProcessId);
NTSTATUS RegisterCowProtectedPage(PFN_NUMBER PageFrameNumber);
NTSTATUS UnregisterCowProtectedPage(PFN_NUMBER PageFrameNumber);
BOOLEAN IsCowProtectionActive();
NTSTATUS DrainDedupeState();
NTSTATUS QueryDedupeHealth(PVOID OutputBuffer, ULONG OutputBufferLength, PULONG_PTR Information);
