#include <ntifs.h>
#include <ntddk.h>
#include <wdf.h>

// Global variables
UNICODE_STRING DEVICE_NAME = RTL_CONSTANT_STRING(L"\\Device\\MyFileNameDetector"); // Driver device name
UNICODE_STRING SYM_LINK = RTL_CONSTANT_STRING(L"\\??\\MyFileNameDetector");        // Device symlink

// Callback for process creation notification
void CreateProcessNotifyRoutine(PEPROCESS process, HANDLE pid, PPS_CREATE_NOTIFY_INFO createInfo) {
    UNREFERENCED_PARAMETER(process);
    UNREFERENCED_PARAMETER(pid);

    // Check for process creation (not exit)
    if (createInfo != NULL && createInfo->CommandLine != NULL) {
        if (wcsstr(createInfo->CommandLine->Buffer, L"notepad") != NULL) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                "MyFileNameDetector: Process (%ws) allowed.\n", createInfo->CommandLine->Buffer);
            createInfo->CreationStatus = STATUS_SUCCESS;
        }
        else if (wcsstr(createInfo->CommandLine->Buffer, L"mimikatz") != NULL) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                "MyFileNameDetector: Process (%ws) denied.\n", createInfo->CommandLine->Buffer);
            createInfo->CreationStatus = STATUS_ACCESS_DENIED;
        }
    }
}

// Unload routine
void UnloadMyDumbEDR(_In_ PDRIVER_OBJECT DriverObject) {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MyFileNameDetector: Unloading routine called\n");

    // Unregister callback
    PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutine, TRUE);

    // Delete symbolic link
    IoDeleteSymbolicLink(&SYM_LINK);

    // Delete device object
    IoDeleteDevice(DriverObject->DeviceObject);
}

// Default unsupported IRP handler
NTSTATUS MyUnsupportedFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_NOT_SUPPORTED;
}

// Entry point
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status;
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING deviceName = DEVICE_NAME;
    UNICODE_STRING symlinkName = SYM_LINK;

    // Assign default handler for all IRPs
    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = MyUnsupportedFunction;
    }

    // Create the device
    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &DeviceObject
    );

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "MyFileNameDetector: Device creation failed: 0x%X\n", status);
        return status;
    }

    // Create the symbolic link
    status = IoCreateSymbolicLink(&symlinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "MyFileNameDetector: Symlink creation failed: 0x%X\n", status);
        IoDeleteDevice(DeviceObject);
        return status;
    }

    // Set the unload routine
    DriverObject->DriverUnload = UnloadMyDumbEDR;

    // Register the process creation callback
    status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutine, FALSE);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "MyFileNameDetector: Failed to register process notify routine: 0x%X\n", status);
        IoDeleteSymbolicLink(&symlinkName);
        IoDeleteDevice(DeviceObject);
        return status;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "MyFileNameDetector: Successfully initialized the driver\n");

    return STATUS_SUCCESS;
}
