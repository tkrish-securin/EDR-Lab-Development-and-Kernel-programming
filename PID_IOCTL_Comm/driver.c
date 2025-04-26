#include <Ntifs.h>
#include <ntddk.h>
#include <wdf.h>

// Global variables
NTSTATUS HandleCustomIOCTL(PDEVICE_OBJECT DeviceObject, PIRP Irp);
volatile ULONG g_LastCreatedPID = 0;
#define IOCTL_SPOTLESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x2049, METHOD_BUFFERED, FILE_ANY_ACCESS)
UNICODE_STRING DEVICE_NAME = RTL_CONSTANT_STRING(L"\\Device\\PIDSenderV2");
UNICODE_STRING SYM_LINK = RTL_CONSTANT_STRING(L"\\??\\PIDSenderV2");

// IOCTL communication for passing info forth & back between kernel and usermode
NTSTATUS HandleCustomIOCTL(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION stackLocation = IoGetCurrentIrpStackLocation(Irp);

    if (stackLocation->Parameters.DeviceIoControl.IoControlCode == IOCTL_SPOTLESS)
    {
        DbgPrint("IOCTL_SPOTLESS (0x%x) issued", stackLocation->Parameters.DeviceIoControl.IoControlCode);

        // Output the last PID to user buffer
        if (stackLocation->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG)) {
            *(ULONG*)Irp->AssociatedIrp.SystemBuffer = g_LastCreatedPID;
            Irp->IoStatus.Information = sizeof(ULONG);
            Irp->IoStatus.Status = STATUS_SUCCESS;
        }
        else {
            Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
        }
    }
    else {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        Irp->IoStatus.Information = 0;
    }

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

// Function to handle opening and closing of comm pipes from usermode 
NTSTATUS MajorFunctions(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stackLocation = IoGetCurrentIrpStackLocation(Irp);

    switch (stackLocation->MajorFunction)
    {
    case IRP_MJ_CREATE:
        DbgPrint("Handle to symbolic link %wZ opened", SYM_LINK);
        break;
    case IRP_MJ_CLOSE:
        DbgPrint("Handle to symbolic link %wZ closed", SYM_LINK);
        break;
    default:
        break;
    }

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

// Handle incoming notifications about new/terminated processes
// Callback for process creation notification
void CreateProcessNotifyRoutine(PEPROCESS process, HANDLE pid, PPS_CREATE_NOTIFY_INFO createInfo) {
    UNREFERENCED_PARAMETER(process);

    if (createInfo != NULL && createInfo->ImageFileName != NULL) {
        g_LastCreatedPID = (ULONG)(ULONG_PTR)pid;  // <-- Save PID
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "MyProcessMonitor: PID %d is launching '%wZ'\n",
            g_LastCreatedPID,
            createInfo->ImageFileName);
        createInfo->CreationStatus = STATUS_SUCCESS;
    }
}

// Unload routine
void UnloadMyDumbEDR(_In_ PDRIVER_OBJECT DriverObject) {
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MyProcessMonitor: Unloading routine called\n");

    // Unregister callback
    PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, TRUE);

    // Delete symbolic link
    IoDeleteSymbolicLink(&SYM_LINK);

    // Delete device object
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = 0;

    // Routine that will execute when our driver is unloaded/service is stopped
    DriverObject->DriverUnload = UnloadMyDumbEDR;

    // Routine for handling IO requests from userland
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HandleCustomIOCTL;

    // Routines that will execute once a handle to our device's symbolic link is opened/closed
    DriverObject->MajorFunction[IRP_MJ_CREATE] = MajorFunctions;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = MajorFunctions;

    DbgPrint("Driver loaded");

    // Create device
    status = IoCreateDevice(DriverObject, 0, &DEVICE_NAME, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &DriverObject->DeviceObject);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("Could not create device %wZ", &DEVICE_NAME);
        return status;
    }
    else
    {
        DbgPrint("Device %wZ created", &DEVICE_NAME);
    }

    // Create symbolic link
    status = IoCreateSymbolicLink(&SYM_LINK, &DEVICE_NAME);
    if (NT_SUCCESS(status))
    {
        DbgPrint("Symbolic link %wZ created", SYM_LINK);
    }
    else
    {
        DbgPrint("Error creating symbolic link %wZ", SYM_LINK);
    }

    // Register process creation notify callback
    status = PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, FALSE);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "MyProcessMonitor: Failed to register process notify routine: 0x%X\n", status);
        IoDeleteSymbolicLink(&SYM_LINK);
        IoDeleteDevice(DriverObject->DeviceObject);
        return status;
    }

    return STATUS_SUCCESS;
}
