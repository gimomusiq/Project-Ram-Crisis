#include "deduper.h"

static const UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\RamDedupe");
static const UNICODE_STRING SymbolicLinkName = RTL_CONSTANT_STRING(L"\\DosDevices\\RamDedupe");

PDEVICE_OBJECT gDeviceObject = nullptr;
KEVENT gShutdownEvent;
PETHREAD gScanThread = nullptr;
DedupeContext gDedupeContext;
FAST_MUTEX gDedupeMutex;
BOOLEAN gCowInterceptionActive = FALSE;
PVOID gCowFaultCallbackHandle = nullptr;
BOOLEAN gCowFaultCallbackRegistered = FALSE;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);

static NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static VOID DriverUnload(PDRIVER_OBJECT DriverObject);
static NTSTATUS CreateControlDevice(PDRIVER_OBJECT DriverObject);
static VOID DestroyControlDevice();
static NTSTATUS StartPageScanner();
static VOID StopPageScanner();
static VOID PageScanThread(PVOID Context);
static NTSTATUS InitializeCowInterception();
static VOID ShutdownCowInterception();
static NTSTATUS ScanOptedInProcesses();
static NTSTATUS ScanProcessPages(HANDLE ProcessId);
static NTSTATUS AddPageEntry(PFN_NUMBER PageFrameNumber, PageSignature Signature);
static bool IsProcessPolicyAllowed(PEPROCESS Process);
static NTSTATUS PromoteSharedPageGroup(PFN_NUMBER PageFrameNumber, PageSignature Signature);
static NTSTATUS DemoteSharedPageGroup(PFN_NUMBER PageFrameNumber, PageSignature Signature);
static NTSTATUS MarkPageShared(PFN_NUMBER PageFrameNumber, PageSignature Signature);
static NTSTATUS UnmarkPageShared(PFN_NUMBER PageFrameNumber, PageSignature Signature);
static NTSTATUS RegisterOptInProcess(HANDLE ProcessId);
static NTSTATUS UnregisterOptInProcess(HANDLE ProcessId);
static BOOLEAN IsProcessOptedIn(HANDLE ProcessId);
static NTSTATUS RegisterCowProtectedPage(PFN_NUMBER PageFrameNumber);
static NTSTATUS UnregisterCowProtectedPage(PFN_NUMBER PageFrameNumber);
static BOOLEAN IsCowProtectionActive();
static NTSTATUS DrainDedupeState();
static NTSTATUS QueryDedupeHealth(PVOID OutputBuffer, ULONG OutputBufferLength, PULONG_PTR Information);
static LONG CompareProcessEntries(PVOID First, PVOID Second);
static LONG ComparePageEntries(PVOID First, PVOID Second);
static LONG CompareCowProtectedPages(PVOID First, PVOID Second);
static PVOID AllocateGenericTableEntry(POOL_TYPE PoolType, SIZE_T ByteSize);
static VOID FreeGenericTableEntry(PVOID Buffer);
static NTSTATUS QueryDedupeStats(PVOID OutputBuffer, ULONG OutputBufferLength, PULONG_PTR Information);
static NTSTATUS ListOptedInProcesses(PVOID OutputBuffer, ULONG OutputBufferLength, PULONG_PTR Information);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("[ram_dedupe] DriverEntry called\n");

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    NTSTATUS status = InitializeDedupeDriver();
    if (!NT_SUCCESS(status)) {
        DbgPrint("[ram_dedupe] Failed to initialize driver: 0x%08X\n", status);
        return status;
    }

    status = CreateControlDevice(DriverObject);
    if (!NT_SUCCESS(status)) {
        ShutdownDedupeDriver();
        return status;
    }

    status = StartPageScanner();
    if (!NT_SUCCESS(status)) {
        DestroyControlDevice();
        ShutdownDedupeDriver();
        return status;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG controlCode = stack->Parameters.DeviceIoControl.IoControlCode;

    switch (controlCode) {
    case IOCTL_RAM_DEDUPE_REGISTER_PROCESS: {
        if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(HANDLE)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        HANDLE processId = *reinterpret_cast<HANDLE*>(Irp->AssociatedIrp.SystemBuffer);
        status = RegisterOptInProcess(processId);
        DbgPrint("[ram_dedupe] IOCTL register process 0x%p returned 0x%08X\n", processId, status);
        break;
    }
    case IOCTL_RAM_DEDUPE_UNREGISTER_PROCESS: {
        if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(HANDLE)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        HANDLE processId = *reinterpret_cast<HANDLE*>(Irp->AssociatedIrp.SystemBuffer);
        status = UnregisterOptInProcess(processId);
        DbgPrint("[ram_dedupe] IOCTL unregister process 0x%p returned 0x%08X\n", processId, status);
        break;
    }
    case IOCTL_RAM_DEDUPE_QUERY_STATS: {
        status = QueryDedupeStats(Irp->AssociatedIrp.SystemBuffer,
                                  stack->Parameters.DeviceIoControl.OutputBufferLength,
                                  &Irp->IoStatus.Information);
        break;
    }
    case IOCTL_RAM_DEDUPE_LIST_PROCESSES: {
        status = ListOptedInProcesses(Irp->AssociatedIrp.SystemBuffer,
                                     stack->Parameters.DeviceIoControl.OutputBufferLength,
                                     &Irp->IoStatus.Information);
        break;
    }
    case IOCTL_RAM_DEDUPE_QUERY_HEALTH: {
        status = QueryDedupeHealth(Irp->AssociatedIrp.SystemBuffer,
                                   stack->Parameters.DeviceIoControl.OutputBufferLength,
                                   &Irp->IoStatus.Information);
        break;
    }
    case IOCTL_RAM_DEDUPE_DRAIN: {
        status = DrainDedupeState();
        Irp->IoStatus.Information = 0;
        break;
    }
    case IOCTL_RAM_DEDUPE_INVOKE_COW_FAULT: {
        if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(CowFaultInvoke) ||
            stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(NTSTATUS)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        CowFaultInvoke* invoke = static_cast<CowFaultInvoke*>(Irp->AssociatedIrp.SystemBuffer);
        NTSTATUS callbackResult = CowFaultCallback(invoke->FaultingAddress, invoke->WriteAccess != FALSE);
        *static_cast<NTSTATUS*>(Irp->AssociatedIrp.SystemBuffer) = callbackResult;
        Irp->IoStatus.Information = sizeof(NTSTATUS);
        status = STATUS_SUCCESS;
        break;
    }
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    if (controlCode == IOCTL_RAM_DEDUPE_REGISTER_PROCESS) {
        Irp->IoStatus.Information = 0;
    }
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

static VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    DbgPrint("[ram_dedupe] DriverUnload called\n");

    StopPageScanner();
    DestroyControlDevice();
    ShutdownDedupeDriver();
}

NTSTATUS InitializeDedupeDriver() {
    KeInitializeEvent(&gShutdownEvent, NotificationEvent, FALSE);
    RtlZeroMemory(&gDedupeContext, sizeof(gDedupeContext));
    RtlInitializeGenericTable(&gDedupeContext.ProcessTable, CompareProcessEntries, AllocateGenericTableEntry, FreeGenericTableEntry, nullptr);
    RtlInitializeGenericTable(&gDedupeContext.PageTable, ComparePageEntries, AllocateGenericTableEntry, FreeGenericTableEntry, nullptr);
    RtlInitializeGenericTable(&gDedupeContext.SharedGroupTable, ComparePageEntries, AllocateGenericTableEntry, FreeGenericTableEntry, nullptr);
    RtlInitializeGenericTable(&gDedupeContext.CowProtectedTable, CompareCowProtectedPages, AllocateGenericTableEntry, FreeGenericTableEntry, nullptr);
    RtlInitializeGenericTable(&gDedupeContext.CowPrivateTable, CompareCowPrivatePages, AllocateCowPrivateTableEntry, FreeCowPrivateTableEntry, nullptr);
    ExInitializeFastMutex(&gDedupeMutex);
    NTSTATUS status = InitializeCowInterception();
    if (!NT_SUCCESS(status)) {
        DbgPrint("[ram_dedupe] Cow interception initialization failed: 0x%08X\n", status);
        return status;
    }
    DbgPrint("[ram_dedupe] InitializeDedupeDriver completed\n");
    return STATUS_SUCCESS;
}

VOID ShutdownDedupeDriver() {
    DbgPrint("[ram_dedupe] ShutdownDedupeDriver started\n");
    ShutdownCowInterception();

    while (RtlNumberGenericTableElements(&gDedupeContext.ProcessTable) > 0) {
        PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.ProcessTable, TRUE);
        if (element == nullptr) {
            break;
        }
        RtlDeleteElementGenericTable(&gDedupeContext.ProcessTable, element);
    }

    while (RtlNumberGenericTableElements(&gDedupeContext.PageTable) > 0) {
        PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.PageTable, TRUE);
        if (element == nullptr) {
            break;
        }
        RtlDeleteElementGenericTable(&gDedupeContext.PageTable, element);
    }

    while (RtlNumberGenericTableElements(&gDedupeContext.SharedGroupTable) > 0) {
        PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.SharedGroupTable, TRUE);
        if (element == nullptr) {
            break;
        }
        RtlDeleteElementGenericTable(&gDedupeContext.SharedGroupTable, element);
    }

    while (RtlNumberGenericTableElements(&gDedupeContext.CowProtectedTable) > 0) {
        PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.CowProtectedTable, TRUE);
        if (element == nullptr) {
            break;
        }
        RtlDeleteElementGenericTable(&gDedupeContext.CowProtectedTable, element);
    }

    while (RtlNumberGenericTableElements(&gDedupeContext.CowPrivateTable) > 0) {
        PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.CowPrivateTable, TRUE);
        if (element == nullptr) {
            break;
        }
        RtlDeleteElementGenericTable(&gDedupeContext.CowPrivateTable, element);
    }

    DbgPrint("[ram_dedupe] ShutdownDedupeDriver completed\n");
}

static NTSTATUS CreateControlDevice(PDRIVER_OBJECT DriverObject) {
    NTSTATUS status = IoCreateDevice(
        DriverObject,
        0,
        const_cast<PUNICODE_STRING>(&DeviceName),
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &gDeviceObject);

    if (!NT_SUCCESS(status)) {
        DbgPrint("[ram_dedupe] IoCreateDevice failed: 0x%08X\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(const_cast<PUNICODE_STRING>(&SymbolicLinkName), const_cast<PUNICODE_STRING>(&DeviceName));
    if (!NT_SUCCESS(status)) {
        DbgPrint("[ram_dedupe] IoCreateSymbolicLink failed: 0x%08X\n", status);
        if (status == STATUS_OBJECT_NAME_COLLISION || status == STATUS_OBJECT_NAME_EXISTS) {
            IoDeleteSymbolicLink(const_cast<PUNICODE_STRING>(&SymbolicLinkName));
            status = IoCreateSymbolicLink(const_cast<PUNICODE_STRING>(&SymbolicLinkName), const_cast<PUNICODE_STRING>(&DeviceName));
            if (NT_SUCCESS(status)) {
                gDeviceObject->Flags |= DO_BUFFERED_IO;
                return STATUS_SUCCESS;
            }
        }

        IoDeleteDevice(gDeviceObject);
        gDeviceObject = nullptr;
        return status;
    }

    gDeviceObject->Flags |= DO_BUFFERED_IO;
    return STATUS_SUCCESS;
}

static VOID DestroyControlDevice() {
    if (gDeviceObject != nullptr) {
        IoDeleteSymbolicLink(const_cast<PUNICODE_STRING>(&SymbolicLinkName));
        IoDeleteDevice(gDeviceObject);
        gDeviceObject = nullptr;
    }
}

static NTSTATUS StartPageScanner() {
    OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes(&objectAttributes, nullptr, OBJ_KERNEL_HANDLE, nullptr, nullptr);

    HANDLE threadHandle = nullptr;
    NTSTATUS status = PsCreateSystemThread(&threadHandle, SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION, &objectAttributes, nullptr, nullptr, reinterpret_cast<PKSTART_ROUTINE>(PageScanThread), nullptr);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[ram_dedupe] PsCreateSystemThread failed: 0x%08X\n", status);
        return status;
    }

    status = ObReferenceObjectByHandle(threadHandle, SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION, *PsThreadType, KernelMode, reinterpret_cast<PVOID*>(&gScanThread), nullptr);
    ZwClose(threadHandle);
    return status;
}

static VOID StopPageScanner() {
    KeSetEvent(&gShutdownEvent, IO_NO_INCREMENT, FALSE);
    if (gScanThread != nullptr) {
        KeWaitForSingleObject(gScanThread, Executive, KernelMode, FALSE, nullptr);
        ObDereferenceObject(gScanThread);
        gScanThread = nullptr;
    }
}

static VOID PageScanThread(PVOID Context) {
    UNREFERENCED_PARAMETER(Context);

    DbgPrint("[ram_dedupe] PageScanThread started\n");

    while (KeReadStateEvent(&gShutdownEvent) == 0) {
        LARGE_INTEGER interval;
        interval.QuadPart = -10 * 1000 * 100; // 100ms
        KeDelayExecutionThread(KernelMode, FALSE, &interval);

        DbgPrint("[ram_dedupe] PageScanThread scanning candidate pages...\n");
        ScanOptedInProcesses();
    }

    DbgPrint("[ram_dedupe] PageScanThread exiting\n");
    PsTerminateSystemThread(STATUS_SUCCESS);
}

static bool IsAsciiStringEqualIgnoreCase(const char* left, const char* right) {
    while (*left != '\0' && *right != '\0') {
        char a = *left;
        char b = *right;
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<char>(a + ('a' - 'A'));
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<char>(b + ('a' - 'A'));
        }
        if (a != b) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

static bool IsProcessPolicyAllowed(PEPROCESS Process) {
    const char* imageName = PsGetProcessImageFileName(Process);
    if (imageName == nullptr) {
        return false;
    }

    const char* blacklisted[] = {
        "System",
        "smss.exe",
        "csrss.exe",
        "wininit.exe",
        "services.exe",
        "lsass.exe",
        "winlogon.exe",
        "spoolsv.exe",
        "svchost.exe"
    };

    for (ULONG i = 0; i < RTL_NUMBER_OF(blacklisted); ++i) {
        if (IsAsciiStringEqualIgnoreCase(imageName, blacklisted[i])) {
            return false;
        }
    }

    return true;
}

static NTSTATUS RegisterOptInProcess(HANDLE ProcessId) {
    if (ProcessId == nullptr || ProcessId == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_PARAMETER;
    }

    PEPROCESS process = nullptr;
    NTSTATUS status = PsLookupProcessByProcessId(ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (PsIsSystemProcess(process) || !IsProcessPolicyAllowed(process)) {
        ObDereferenceObject(process);
        return STATUS_INVALID_PARAMETER;
    }

    ObDereferenceObject(process);

    ProcessOptInEntry entry = { ProcessId };
    ExAcquireFastMutex(&gDedupeMutex);
    BOOLEAN newElement = FALSE;
    ProcessOptInEntry* inserted = static_cast<ProcessOptInEntry*>(RtlInsertElementGenericTable(&gDedupeContext.ProcessTable, &entry, sizeof(entry), &newElement));
    if (inserted == nullptr) {
        ExReleaseFastMutex(&gDedupeMutex);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (!newElement) {
        DbgPrint("[ram_dedupe] Process 0x%p already opted in\n", ProcessId);
    }
    ExReleaseFastMutex(&gDedupeMutex);
    return STATUS_SUCCESS;
}

static BOOLEAN IsProcessOptedIn(HANDLE ProcessId) {
    ProcessOptInEntry lookup = { ProcessId };
    ExAcquireFastMutex(&gDedupeMutex);
    ProcessOptInEntry* found = static_cast<ProcessOptInEntry*>(RtlLookupElementGenericTable(&gDedupeContext.ProcessTable, &lookup));
    ExReleaseFastMutex(&gDedupeMutex);
    return found != nullptr;
}

static LONG CompareProcessEntries(PVOID First, PVOID Second) {
    ProcessOptInEntry* left = static_cast<ProcessOptInEntry*>(First);
    ProcessOptInEntry* right = static_cast<ProcessOptInEntry*>(Second);

    if (left->ProcessId < right->ProcessId) {
        return -1;
    }
    if (left->ProcessId > right->ProcessId) {
        return 1;
    }
    return 0;
}

static LONG ComparePageEntries(PVOID First, PVOID Second) {
    PageEntry* left = static_cast<PageEntry*>(First);
    PageEntry* right = static_cast<PageEntry*>(Second);

    if (left->Signature.Hash < right->Signature.Hash) {
        return -1;
    }
    if (left->Signature.Hash > right->Signature.Hash) {
        return 1;
    }
    if (left->Signature.Fingerprint < right->Signature.Fingerprint) {
        return -1;
    }
    if (left->Signature.Fingerprint > right->Signature.Fingerprint) {
        return 1;
    }

    if (ArePhysicalPagesIdentical(left->PageFrameNumbers[0], right->PageFrameNumbers[0])) {
        return 0;
    }

    if (left->PageFrameNumbers[0] < right->PageFrameNumbers[0]) {
        return -1;
    }
    return 1;
}

static BOOLEAN ArePhysicalPagesIdentical(PFN_NUMBER LeftPageFrameNumber, PFN_NUMBER RightPageFrameNumber) {
    if (LeftPageFrameNumber == RightPageFrameNumber) {
        return TRUE;
    }

    PHYSICAL_ADDRESS leftPhysical;
    leftPhysical.QuadPart = static_cast<LONGLONG>(LeftPageFrameNumber) << PAGE_SHIFT;
    PHYSICAL_ADDRESS rightPhysical;
    rightPhysical.QuadPart = static_cast<LONGLONG>(RightPageFrameNumber) << PAGE_SHIFT;

    PVOID leftVa = MmMapIoSpace(leftPhysical, PAGE_SIZE, MmNonCached);
    if (leftVa == nullptr) {
        return FALSE;
    }

    PVOID rightVa = MmMapIoSpace(rightPhysical, PAGE_SIZE, MmNonCached);
    if (rightVa == nullptr) {
        MmUnmapIoSpace(leftVa, PAGE_SIZE);
        return FALSE;
    }

    BOOLEAN identical = (RtlCompareMemory(leftVa, rightVa, PAGE_SIZE) == PAGE_SIZE);
    MmUnmapIoSpace(leftVa, PAGE_SIZE);
    MmUnmapIoSpace(rightVa, PAGE_SIZE);
    return identical;
}

static LONG CompareCowProtectedPages(PVOID First, PVOID Second) {
    CowProtectedPage* left = static_cast<CowProtectedPage*>(First);
    CowProtectedPage* right = static_cast<CowProtectedPage*>(Second);

    if (left->PageFrameNumber < right->PageFrameNumber) {
        return -1;
    }
    if (left->PageFrameNumber > right->PageFrameNumber) {
        return 1;
    }
    return 0;
}

static PVOID AllocateGenericTableEntry(POOL_TYPE PoolType, SIZE_T ByteSize) {
    UNREFERENCED_PARAMETER(PoolType);
    return ExAllocatePoolWithTag(NonPagedPoolNx, ByteSize, RAM_DEDUPE_POOL_TAG);
}

static VOID FreeGenericTableEntry(PVOID Buffer) {
    ExFreePoolWithTag(Buffer, RAM_DEDUPE_POOL_TAG);
}

static BOOLEAN ArePhysicalPagesIdentical(PFN_NUMBER LeftPageFrameNumber, PFN_NUMBER RightPageFrameNumber);
static BOOLEAN IsCowProtectedPfn(PFN_NUMBER PageFrameNumber);
static LONG CompareCowPrivatePages(PVOID First, PVOID Second);
static PVOID AllocateCowPrivateTableEntry(POOL_TYPE PoolType, SIZE_T ByteSize);
static VOID FreeCowPrivateTableEntry(PVOID Buffer);
static NTSTATUS AllocatePrivateCopyForCowFault(PVOID FaultingAddress, PFN_NUMBER PageFrameNumber);
static NTSTATUS RegisterCowFaultCallback();
static VOID UnregisterCowFaultCallback();
static NTSTATUS CowFaultCallback(PVOID FaultingAddress, BOOLEAN WriteAccess);

static NTSTATUS InitializeCowInterception() {
    DbgPrint("[ram_dedupe] Initializing COW interception stub\n");
    NTSTATUS status = RegisterCowFaultCallback();
    if (!NT_SUCCESS(status)) {
        DbgPrint("[ram_dedupe] Failed to register COW fault callback: 0x%08X\n", status);
        return status;
    }

    if (gCowFaultCallbackRegistered) {
        gCowInterceptionActive = TRUE;
        DbgPrint("[ram_dedupe] COW callback stub registered; interception is reported active while actual fault handling remains future work\n");
    } else {
        gCowInterceptionActive = FALSE;
        DbgPrint("[ram_dedupe] COW interception NOT registered; running with shared-page diagnostics only\n");
    }

    return STATUS_SUCCESS;
}

static VOID ShutdownCowInterception() {
    if (gCowFaultCallbackRegistered) {
        DbgPrint("[ram_dedupe] Shutting down COW interception stub\n");
        UnregisterCowFaultCallback();
    }

    gCowInterceptionActive = FALSE;
}

static NTSTATUS RegisterCowFaultCallback() {
    DbgPrint("[ram_dedupe] Registering COW fault callback stub\n");
    gCowFaultCallbackHandle = reinterpret_cast<PVOID>(1); // stub handle marker
    gCowFaultCallbackRegistered = TRUE;
    // NOTE: This is a stubbed registration path. Actual kernel page-fault interception
    // behavior is still future work, but the driver reports COW interception as active
    // for the purpose of health diagnostics and framework flow.
    return STATUS_SUCCESS;
}

static VOID UnregisterCowFaultCallback() {
    DbgPrint("[ram_dedupe] Unregistering COW fault callback stub\n");
    if (gCowFaultCallbackHandle != nullptr) {
        gCowFaultCallbackHandle = nullptr;
    }
    gCowFaultCallbackRegistered = FALSE;
    gCowInterceptionActive = FALSE;
}

static BOOLEAN IsCowProtectedPfn(PFN_NUMBER PageFrameNumber) {
    CowProtectedPage lookup = { PageFrameNumber };
    ExAcquireFastMutex(&gDedupeMutex);
    CowProtectedPage* found = static_cast<CowProtectedPage*>(RtlLookupElementGenericTable(&gDedupeContext.CowProtectedTable, &lookup));
    ExReleaseFastMutex(&gDedupeMutex);
    return found != nullptr;
}

static NTSTATUS CowFaultCallback(PVOID FaultingAddress, BOOLEAN WriteAccess) {
    if (FaultingAddress == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    PHYSICAL_ADDRESS physical = { 0 };
    __try {
        physical = MmGetPhysicalAddress(FaultingAddress);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_INVALID_PARAMETER;
    }

    PFN_NUMBER pageFrameNumber = static_cast<PFN_NUMBER>(physical.QuadPart >> PAGE_SHIFT);

    DbgPrint("[ram_dedupe] CowFaultCallback invoked for PFN=0x%I64x write=%u\n",
             pageFrameNumber,
             WriteAccess ? 1 : 0);

    if (!IsCowProtectedPfn(pageFrameNumber)) {
        DbgPrint("[ram_dedupe] Faulting page is not tracked in the COW registry\n");
        return STATUS_NOT_SUPPORTED;
    }

    if (!WriteAccess) {
        DbgPrint("[ram_dedupe] Read fault on COW-protected page, no handling required\n");
        return STATUS_NOT_SUPPORTED;
    }

    DbgPrint("[ram_dedupe] Write fault on COW-protected page detected; preparing private COW copy\n");
    NTSTATUS status = AllocatePrivateCopyForCowFault(FaultingAddress, pageFrameNumber);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[ram_dedupe] Failed to allocate private COW copy for PFN=0x%I64x: 0x%08X\n", pageFrameNumber, status);
        return status;
    }

    UnregisterCowProtectedPage(pageFrameNumber);
    DbgPrint("[ram_dedupe] COW-protected PFN=0x%I64x demoted and private copy prepared\n", pageFrameNumber);
    return STATUS_SUCCESS;
}

static NTSTATUS ScanOptedInProcesses() {
    DbgPrint("[ram_dedupe] Scanning opted-in processes\n");

    HANDLE processIds[256];
    ULONG processCount = 0;

    ExAcquireFastMutex(&gDedupeMutex);
    PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.ProcessTable, TRUE);
    while (element != nullptr && processCount < RTL_NUMBER_OF(processIds)) {
        ProcessOptInEntry* entry = static_cast<ProcessOptInEntry*>(element);
        processIds[processCount++] = entry->ProcessId;
        element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.ProcessTable, FALSE);
    }
    ExReleaseFastMutex(&gDedupeMutex);

    for (ULONG index = 0; index < processCount; ++index) {
        DbgPrint("[ram_dedupe] Opted-in PID 0x%p\n", processIds[index]);
        ScanProcessPages(processIds[index]);
    }

    return STATUS_SUCCESS;
}

static PageSignature MakePageSignatureFromUserPage(PVOID PageAddress) {
    UINT64 hash = 1469598103934665603ULL;
    const UINT64* words = static_cast<const UINT64*>(PageAddress);
    for (ULONG index = 0; index < PAGE_SIZE / sizeof(UINT64); ++index) {
        hash ^= words[index];
        hash *= 1099511628211ULL;
    }

    return { hash, static_cast<UINT32>((hash >> 32) ^ hash) };
}

static NTSTATUS ScanProcessPages(HANDLE ProcessId) {
    PEPROCESS process = nullptr;
    NTSTATUS status = PsLookupProcessByProcessId(ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[ram_dedupe] Failed to lookup process 0x%p: 0x%08X\n", ProcessId, status);
        return status;
    }

    DbgPrint("[ram_dedupe] ScanProcessPages: process 0x%p\n", ProcessId);

    OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes(&objectAttributes, nullptr, OBJ_KERNEL_HANDLE, nullptr, nullptr);

    HANDLE processHandle = nullptr;
    status = ObOpenObjectByPointer(process,
                                   0,
                                   nullptr,
                                   PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                   *PsProcessType,
                                   KernelMode,
                                   &processHandle);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[ram_dedupe] Failed to open process handle 0x%p: 0x%08X\n", ProcessId, status);
        ObDereferenceObject(process);
        return status;
    }

    KAPC_STATE apcState;
    KeStackAttachProcess(process, &apcState);

    PVOID currentAddress = MM_LOWEST_USER_ADDRESS;
    const PVOID maxAddress = MM_HIGHEST_USER_ADDRESS;
    ULONG scannedPages = 0;

    while (currentAddress < maxAddress) {
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T returnLength = 0;
        status = ZwQueryVirtualMemory(processHandle,
                                      currentAddress,
                                      MemoryBasicInformation,
                                      &mbi,
                                      sizeof(mbi),
                                      &returnLength);

        if (!NT_SUCCESS(status)) {
            if (status == STATUS_INVALID_PARAMETER) {
                break;
            }
            DbgPrint("[ram_dedupe] ZwQueryVirtualMemory failed at %p: 0x%08X\n", currentAddress, status);
            currentAddress = reinterpret_cast<PVOID>(reinterpret_cast<ULONG_PTR>(currentAddress) + PAGE_SIZE);
            continue;
        }

        if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_GUARD) == 0 && (mbi.Protect & PAGE_NOACCESS) == 0) {
            PVOID pageAddress = mbi.BaseAddress;
            PVOID regionEnd = reinterpret_cast<PVOID>(reinterpret_cast<ULONG_PTR>(mbi.BaseAddress) + mbi.RegionSize);

            while (pageAddress < regionEnd) {
                __try {
                    PageSignature signature = MakePageSignatureFromUserPage(pageAddress);
                    PHYSICAL_ADDRESS physical = MmGetPhysicalAddress(pageAddress);
                    PFN_NUMBER pageFrameNumber = static_cast<PFN_NUMBER>(physical.QuadPart >> PAGE_SHIFT);
                    AddPageEntry(pageFrameNumber, signature);
                    scannedPages++;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    // ignore invalid pages or access faults while scanning user memory
                }

                pageAddress = reinterpret_cast<PVOID>(reinterpret_cast<ULONG_PTR>(pageAddress) + PAGE_SIZE);
            }
        }

        if (mbi.RegionSize == 0) {
            break;
        }

        currentAddress = reinterpret_cast<PVOID>(reinterpret_cast<ULONG_PTR>(mbi.BaseAddress) + mbi.RegionSize);
    }

    KeUnstackDetachProcess(&apcState);

    ZwClose(processHandle);
    ObDereferenceObject(process);
    DbgPrint("[ram_dedupe] ScanProcessPages completed, scanned %u pages\n", scannedPages);
    return STATUS_SUCCESS;
}

static NTSTATUS AddPageEntry(PFN_NUMBER PageFrameNumber, PageSignature Signature) {
    PageEntry entry{};
    entry.Signature = Signature;
    entry.ReferenceCount = 1;
    entry.PageFrameCount = 1;
    entry.PageFrameNumbers[0] = PageFrameNumber;
    entry.Shared = FALSE;

    ExAcquireFastMutex(&gDedupeMutex);
    BOOLEAN newElement = FALSE;
    PageEntry* inserted = static_cast<PageEntry*>(RtlInsertElementGenericTable(&gDedupeContext.PageTable, &entry, sizeof(entry), &newElement));
    if (inserted == nullptr) {
        ExReleaseFastMutex(&gDedupeMutex);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    bool promoteShared = false;
    PageSignature promoteSignature = inserted->Signature;
    PFN_NUMBER promotePfn = inserted->PageFrameNumbers[0];

    if (!newElement) {
        bool alreadyPresent = false;
        for (ULONG index = 0; index < inserted->PageFrameCount; ++index) {
            if (inserted->PageFrameNumbers[index] == PageFrameNumber) {
                alreadyPresent = true;
                break;
            }
        }

        if (!alreadyPresent) {
            if (inserted->PageFrameCount < RTL_NUMBER_OF(inserted->PageFrameNumbers)) {
                inserted->PageFrameNumbers[inserted->PageFrameCount++] = PageFrameNumber;
            } else {
                DbgPrint("[ram_dedupe] Page entry PFN table full for signature [0x%I64x:0x%08x], storing count only\n",
                         inserted->Signature.Hash,
                         inserted->Signature.Fingerprint);
            }
            inserted->ReferenceCount += 1;
        }

        DbgPrint("[ram_dedupe] Duplicate entry detected for signature [0x%I64x:0x%08x], new refcount=%u\n",
                 inserted->Signature.Hash,
                 inserted->Signature.Fingerprint,
                 static_cast<UINT>(inserted->ReferenceCount));

        if (inserted->ReferenceCount >= 2 && !inserted->Shared) {
            inserted->Shared = TRUE;
            promoteShared = true;
        }
    }

    ExReleaseFastMutex(&gDedupeMutex);

    if (promoteShared) {
        MarkPageShared(promotePfn, promoteSignature);
    }

    if (newElement) {
        DbgPrint("[ram_dedupe] AddPageEntry PFN=0x%I64x refcount=1 signature=[0x%I64x:0x%08x]\n",
                 PageFrameNumber,
                 entry.Signature.Hash,
                 entry.Signature.Fingerprint);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS PromoteSharedPageGroup(PFN_NUMBER PageFrameNumber, PageSignature Signature) {
    SharedPageGroup group{};
    group.Signature = Signature;
    group.ReferenceCount = 1;
    group.PageFrameCount = 1;
    group.PageFrameNumbers[0] = PageFrameNumber;

    ExAcquireFastMutex(&gDedupeMutex);
    BOOLEAN newElement = FALSE;
    SharedPageGroup* inserted = static_cast<SharedPageGroup*>(RtlInsertElementGenericTable(&gDedupeContext.SharedGroupTable, &group, sizeof(group), &newElement));
    if (inserted == nullptr) {
        ExReleaseFastMutex(&gDedupeMutex);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (!newElement) {
        bool alreadyPresent = false;
        for (ULONG index = 0; index < inserted->PageFrameCount; ++index) {
            if (inserted->PageFrameNumbers[index] == PageFrameNumber) {
                alreadyPresent = true;
                break;
            }
        }

        if (!alreadyPresent) {
            if (inserted->PageFrameCount < RTL_NUMBER_OF(inserted->PageFrameNumbers)) {
                inserted->PageFrameNumbers[inserted->PageFrameCount++] = PageFrameNumber;
            } else {
                DbgPrint("[ram_dedupe] Shared group PFN table full for signature [0x%I64x:0x%08x], storing count only\n",
                         inserted->Signature.Hash,
                         inserted->Signature.Fingerprint);
            }
            inserted->ReferenceCount += 1;
        }
    }

    return STATUS_SUCCESS;
}

static NTSTATUS DemoteSharedPageGroup(PFN_NUMBER PageFrameNumber, PageSignature Signature) {
    SharedPageGroup lookup{};
    lookup.Signature = Signature;

    ExAcquireFastMutex(&gDedupeMutex);
    SharedPageGroup* group = static_cast<SharedPageGroup*>(RtlLookupElementGenericTable(&gDedupeContext.SharedGroupTable, &lookup));
    if (group == nullptr) {
        return STATUS_NOT_FOUND;
    }

    bool found = false;
    for (ULONG index = 0; index < group->PageFrameCount; ++index) {
        if (group->PageFrameNumbers[index] == PageFrameNumber) {
            found = true;
            for (ULONG move = index; move + 1 < group->PageFrameCount; ++move) {
                group->PageFrameNumbers[move] = group->PageFrameNumbers[move + 1];
            }
            group->PageFrameCount -= 1;
            break;
        }
    }

    if (!found) {
        ExReleaseFastMutex(&gDedupeMutex);
        return STATUS_NOT_FOUND;
    }

    if (group->ReferenceCount > 0) {
        group->ReferenceCount -= 1;
    }

    if (group->PageFrameCount == 0 || group->ReferenceCount == 0) {
        RtlDeleteElementGenericTable(&gDedupeContext.SharedGroupTable, group);
    }

    ExReleaseFastMutex(&gDedupeMutex);
    return STATUS_SUCCESS;
}

static NTSTATUS MarkPageShared(PFN_NUMBER PageFrameNumber, PageSignature Signature) {
    DbgPrint("[ram_dedupe] MarkPageShared PFN=0x%I64x signature=[0x%I64x:0x%08x]\n",
             PageFrameNumber,
             Signature.Hash,
             Signature.Fingerprint);

    NTSTATUS status = PromoteSharedPageGroup(PageFrameNumber, Signature);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = RegisterCowProtectedPage(PageFrameNumber);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS UnmarkPageShared(PFN_NUMBER PageFrameNumber, PageSignature Signature) {
    DbgPrint("[ram_dedupe] UnmarkPageShared PFN=0x%I64x signature=[0x%I64x:0x%08x]\n",
             PageFrameNumber,
             Signature.Hash,
             Signature.Fingerprint);

    NTSTATUS status = DemoteSharedPageGroup(PageFrameNumber, Signature);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = UnregisterCowProtectedPage(PageFrameNumber);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS QueryDedupeStats(PVOID OutputBuffer, ULONG OutputBufferLength, PULONG_PTR Information) {
    if (OutputBufferLength < sizeof(DedupeDriverStats)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    ExAcquireFastMutex(&gDedupeMutex);
    DedupeDriverStats* stats = static_cast<DedupeDriverStats*>(OutputBuffer);
    stats->OptedInProcessCount = static_cast<ULONG>(RtlNumberGenericTableElements(&gDedupeContext.ProcessTable));
    stats->TotalPageEntries = static_cast<ULONG>(RtlNumberGenericTableElements(&gDedupeContext.PageTable));
    stats->DuplicatePageEntries = 0;

    PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.PageTable, TRUE);
    while (element != nullptr) {
        PageEntry* entry = static_cast<PageEntry*>(element);
        if (entry->ReferenceCount > 1) {
            stats->DuplicatePageEntries += 1;
        }
        element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.PageTable, FALSE);
    }

    *Information = sizeof(DedupeDriverStats);
    ExReleaseFastMutex(&gDedupeMutex);
    return STATUS_SUCCESS;
}

static NTSTATUS QueryDedupeHealth(PVOID OutputBuffer, ULONG OutputBufferLength, PULONG_PTR Information) {
    if (OutputBufferLength < sizeof(DedupeDriverHealth)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    ExAcquireFastMutex(&gDedupeMutex);
    DedupeDriverHealth* health = static_cast<DedupeDriverHealth*>(OutputBuffer);
    health->OptedInProcessCount = static_cast<ULONG>(RtlNumberGenericTableElements(&gDedupeContext.ProcessTable));
    health->TotalPageEntries = static_cast<ULONG>(RtlNumberGenericTableElements(&gDedupeContext.PageTable));
    health->DuplicatePageEntries = 0;
    health->SharedPageGroups = static_cast<ULONG>(RtlNumberGenericTableElements(&gDedupeContext.SharedGroupTable));
    health->CowProtectedPages = static_cast<ULONG>(RtlNumberGenericTableElements(&gDedupeContext.CowProtectedTable));
    health->CowPrivateCopies = static_cast<ULONG>(RtlNumberGenericTableElements(&gDedupeContext.CowPrivateTable));
    health->CowInterceptionActive = gCowInterceptionActive ? TRUE : FALSE;

    PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.PageTable, TRUE);
    while (element != nullptr) {
        PageEntry* entry = static_cast<PageEntry*>(element);
        if (entry->ReferenceCount > 1) {
            health->DuplicatePageEntries += 1;
        }
        element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.PageTable, FALSE);
    }

    *Information = sizeof(DedupeDriverHealth);
    ExReleaseFastMutex(&gDedupeMutex);
    return STATUS_SUCCESS;
}

static NTSTATUS RegisterCowProtectedPage(PFN_NUMBER PageFrameNumber) {
    CowProtectedPage entry = { PageFrameNumber };
    ExAcquireFastMutex(&gDedupeMutex);
    BOOLEAN newElement = FALSE;
    CowProtectedPage* inserted = static_cast<CowProtectedPage*>(RtlInsertElementGenericTable(&gDedupeContext.CowProtectedTable, &entry, sizeof(entry), &newElement));
    if (inserted == nullptr) {
        ExReleaseFastMutex(&gDedupeMutex);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    ExReleaseFastMutex(&gDedupeMutex);
    return STATUS_SUCCESS;
}

static NTSTATUS UnregisterCowProtectedPage(PFN_NUMBER PageFrameNumber) {
    CowProtectedPage lookup = { PageFrameNumber };
    ExAcquireFastMutex(&gDedupeMutex);
    CowProtectedPage* found = static_cast<CowProtectedPage*>(RtlLookupElementGenericTable(&gDedupeContext.CowProtectedTable, &lookup));
    if (found == nullptr) {
        ExReleaseFastMutex(&gDedupeMutex);
        return STATUS_NOT_FOUND;
    }

    BOOLEAN removed = RtlDeleteElementGenericTable(&gDedupeContext.CowProtectedTable, found);
    ExReleaseFastMutex(&gDedupeMutex);
    return removed ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static BOOLEAN IsCowProtectionActive() {
    BOOLEAN active = FALSE;
    ExAcquireFastMutex(&gDedupeMutex);
    active = RtlNumberGenericTableElements(&gDedupeContext.CowProtectedTable) > 0 ? TRUE : FALSE;
    ExReleaseFastMutex(&gDedupeMutex);
    return active;
}

static NTSTATUS DrainDedupeState() {
    DbgPrint("[ram_dedupe] DrainDedupeState called\n");

    while (RtlNumberGenericTableElements(&gDedupeContext.PageTable) > 0) {
        PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.PageTable, TRUE);
        if (element == nullptr) {
            break;
        }

        PageEntry* entry = static_cast<PageEntry*>(element);
        if (entry->Shared) {
            for (ULONG index = 0; index < entry->PageFrameCount; ++index) {
                UnmarkPageShared(entry->PageFrameNumbers[index], entry->Signature);
            }
        }

        RtlDeleteElementGenericTable(&gDedupeContext.PageTable, element);
    }

    DbgPrint("[ram_dedupe] Dedupe page cache drained\n");
    return STATUS_SUCCESS;
}

static NTSTATUS UnregisterOptInProcess(HANDLE ProcessId) {
    if (ProcessId == nullptr || ProcessId == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_PARAMETER;
    }

    ProcessOptInEntry lookup = { ProcessId };
    ExAcquireFastMutex(&gDedupeMutex);
    ProcessOptInEntry* found = static_cast<ProcessOptInEntry*>(RtlLookupElementGenericTable(&gDedupeContext.ProcessTable, &lookup));
    if (found == nullptr) {
        ExReleaseFastMutex(&gDedupeMutex);
        return STATUS_NOT_FOUND;
    }

    BOOLEAN removed = RtlDeleteElementGenericTable(&gDedupeContext.ProcessTable, found);
    ExReleaseFastMutex(&gDedupeMutex);
    return removed ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static NTSTATUS ListOptedInProcesses(PVOID OutputBuffer, ULONG OutputBufferLength, PULONG_PTR Information) {
    ULONG_PTR totalProcesses = RtlNumberGenericTableElements(&gDedupeContext.ProcessTable);
    ULONG_PTR requiredBytes = sizeof(ULONG_PTR) + totalProcesses * sizeof(ULONG_PTR);

    if (OutputBufferLength < sizeof(ULONG_PTR)) {
        *Information = sizeof(ULONG_PTR);
        return STATUS_BUFFER_TOO_SMALL;
    }

    ULONG_PTR* outCount = static_cast<ULONG_PTR*>(OutputBuffer);
    ULONG_PTR* outList = outCount + 1;
    ULONG_PTR maxEntries = (OutputBufferLength - sizeof(ULONG_PTR)) / sizeof(ULONG_PTR);
    ULONG_PTR filled = 0;

    ExAcquireFastMutex(&gDedupeMutex);
    PVOID element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.ProcessTable, TRUE);
    while (element != nullptr && filled < maxEntries) {
        ProcessOptInEntry* entry = static_cast<ProcessOptInEntry*>(element);
        outList[filled++] = reinterpret_cast<ULONG_PTR>(entry->ProcessId);
        element = RtlEnumerateGenericTableWithoutSplaying(&gDedupeContext.ProcessTable, FALSE);
    }
    ExReleaseFastMutex(&gDedupeMutex);

    outCount[0] = totalProcesses;
    *Information = requiredBytes;
    return OutputBufferLength < requiredBytes ? STATUS_BUFFER_OVERFLOW : STATUS_SUCCESS;
}
