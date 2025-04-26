#include <iostream>
#include <Windows.h>
#include <vector>  // To store PIDs

#define IOCTL_SPOTLESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x2049, METHOD_BUFFERED, FILE_ANY_ACCESS)

int main(int argc, char** argv)
{
    HANDLE device = INVALID_HANDLE_VALUE;
    BOOL status = FALSE;
    DWORD bytesReturned = 0;
    ULONG pid = 0;

    // Vector to store received PIDs
    std::vector<ULONG> pidList;

    // Open the device
    device = CreateFileW(L"\\\\.\\PIDSenderV2", GENERIC_WRITE | GENERIC_READ | GENERIC_EXECUTE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, 0);

    if (device == INVALID_HANDLE_VALUE)
    {
        printf_s("> Could not open device: 0x%x\n", GetLastError());
        return FALSE;
    }

        printf_s("> Issuing IOCTL_SPOTLESS 0x%x\n", IOCTL_SPOTLESS);
        status = DeviceIoControl(device, IOCTL_SPOTLESS, nullptr, 0, &pid, sizeof(pid), &bytesReturned, nullptr);

        if (status && bytesReturned == sizeof(ULONG)) {
            printf_s("> Received PID from kernel: %lu\n", pid);
            // Store the received PID in the vector
        }
        else {
            printf_s("> Failed to receive PID, error: %d\n", GetLastError());
        }

    // Close the device handle
    CloseHandle(device);

    return 0;
}
