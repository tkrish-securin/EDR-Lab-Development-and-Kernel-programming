#include <Ntifs.h>
#include <ntddk.h>
#include <wdf.h>

// Global variables
UNICODE_STRING DEVICE_NAME = RTL_CONSTANT_STRING(L"\\Device\\MyFirstDriver"); // Driver device name
UNICODE_STRING SYM_LINK = RTL_CONSTANT_STRING(L"\\??\\MyFirstDriver");        // Device symlink
// handle incoming notifications about new/terminated processes
void CreateProcessNotifyRoutine(HANDLE ppid, HANDLE pid, BOOLEAN create) {
    if (create) {
        PEPROCESS process = NULL;
        PUNICODE_STRING processName = NULL;
        //UNREFERENCED_PARAMETER(ppid);

        // Retrieve process ID
        PsLookupProcessByProcessId(pid, &process);

        // Retrieve the process name from the EPROCESS structure
        SeLocateProcessImageName(process, &processName);

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "MyDumbEDR: %d (%wZ) launched.\n", pid, processName);
    }
    else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "MyDumbEDR: %d got killed.\n", pid);
    }
}

void UnloadMyDumbEDR(_In_ PDRIVER_OBJECT DriverObject) {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MyFirstDriver: Unloading routine called\n");
     // Unset the callback
    PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, TRUE);
    // Delete the driver device 
    IoDeleteDevice(DriverObject->DeviceObject);
    // Delete the symbolic link
    IoDeleteSymbolicLink(&SYM_LINK);
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

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MyFirstDriver: Initializing the driver\n");

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
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "MyFirstDriver: Device creation failed: 0x%X\n", status);
        return status;
    }

    // Creating the symlink that we will use to contact our driver
    status = IoCreateSymbolicLink(
        &symlinkName, // The symbolic link name
        &deviceName   // The device name
    );

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "MyFirstDriver: Symlink creation failed\n");
        IoDeleteDevice(DeviceObject);
        return status;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MyFirstDriver: successfully initialized the driver\n");

    // Setting the unload routine to execute
    DriverObject->DriverUnload = UnloadMyDumbEDR;
    PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, FALSE);

    return status;
}
