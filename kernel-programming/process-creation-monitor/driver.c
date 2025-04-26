#include <Ntifs.h>
#include <ntddk.h>
#include <wdf.h>

// Global variables
UNICODE_STRING DEVICE_NAME = RTL_CONSTANT_STRING(L"\\Device\\MyProcessMonitor"); // Driver device name
UNICODE_STRING SYM_LINK = RTL_CONSTANT_STRING(L"\\??\\MyProcessMonitor");        // Device symlink
// handle incoming notifications about new/terminated processes
// Callback for process creation notification
void CreateProcessNotifyRoutine(PEPROCESS process, HANDLE pid, PPS_CREATE_NOTIFY_INFO createInfo) {
    UNREFERENCED_PARAMETER(process);
    UNREFERENCED_PARAMETER(pid);

    // Check for process creation (not exit)
    if (createInfo != NULL && createInfo->ImageFileName != NULL) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "MyProcessMonitor: PID %d is launching '%wZ'\n",
            (ULONG)(ULONG_PTR)pid,
            createInfo->ImageFileName);
        createInfo->CreationStatus = STATUS_SUCCESS;
    }
    else {
        // Process is terminating
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "MyProcessMonitor: PID %d terminated.\n", (ULONG)(ULONG_PTR)pid);
    }
}
// Unload routine
void UnloadMyDumbEDR(_In_ PDRIVER_OBJECT DriverObject) {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MyProcessMonitor: Unloading routine called\n");

    // Unregister callback
    PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutine, TRUE);

    // Delete symbolic link
    IoDeleteSymbolicLink(&SYM_LINK);

    // Delete device object
    IoDeleteDevice(DriverObject->DeviceObject);
}
NTSTATUS MyUnsupportedFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    UNREFERENCED_PARAMETER(DeviceObject);

    return STATUS_NOT_SUPPORTED;
}


NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {


    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = MyUnsupportedFunction;
    }

    // Prevent compiler error in level 4 warnings
    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MyProcessMonitor: Initializing the driver\n");

    // Variable that will store the output of WinAPI functions
    NTSTATUS status;

    // Initializing a device object and creating it
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING deviceName = DEVICE_NAME;
    UNICODE_STRING symlinkName = SYM_LINK;
    status = IoCreateDevice(
        DriverObject,		    // Our driver object
        0,					    // Extra bytes needed (we don't need any)
        &deviceName,            // The device name
        FILE_DEVICE_UNKNOWN,    // The device type
        0,					    // Device characteristics (none)
        FALSE,				    // Sets the driver to not exclusive
        &DeviceObject		    // Pointer in which is stored the result of IoCreateDevice
    );

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "MyProcessMonitor: Device creation failed: 0x%X\n", status);
        return status;
    }

    // Creating the symlink that we will use to contact our driver
    status = IoCreateSymbolicLink(
        &symlinkName, // The symbolic link name
        &deviceName   // The device name
    );

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "MyProcessMonitor: Symlink creation failed\n");
        IoDeleteDevice(DeviceObject);
        return status;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MyProcessMonitor: successfully initialized the driver\n");

    // Setting the unload routine to execute
    DriverObject->DriverUnload = UnloadMyDumbEDR;
    // Register the process creation callback
    status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutine, FALSE);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "MyProcessMonitor: Failed to register process notify routine: 0x%X\n", status);
        IoDeleteSymbolicLink(&symlinkName);
        IoDeleteDevice(DeviceObject);
        return status;
    }

    return STATUS_SUCCESS;
}
