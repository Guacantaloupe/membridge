/*
 * MembridgePhys - minimal HVCI-compatible physical memory provider driver.
 *
 * Design goals:
 *   - No writable+executable sections, no dynamic code, no self-modifying code.
 *   - Only MmMapIoSpace/RtlCopyMemory; no user-mode mapping of executable pages.
 *   - Simple WDM control device: \\.\MembridgePhys
 *
 * IOCTLs (METHOD_BUFFERED):
 *   0x80002000  READ : input {u64 Phys; u32 Size; u32 Reserved}; output Size bytes
 *   0x80002004  WRITE: input {u64 Phys; u32 Size; u32 Reserved; u8 Data[Size]}
 *   0x80002008  QUERY: output {u32 Version}
 *
 * HVCI note: this driver is *source-compatible* with HVCI (no W^X violations),
 * but under HVCI it must be signed by Microsoft (WHQL/attestation) to load.
 * Test signing / unsigned builds do not load while HVCI is enabled.
 */

#include <ntddk.h>

#define MEMBRIDGE_PHYS_DEVICE_NAME  L"\\Device\\MembridgePhys"
#define MEMBRIDGE_PHYS_DOS_NAME     L"\\DosDevices\\MembridgePhys"

#define IOCTL_MB_READ   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MB_WRITE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MB_QUERY  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MB_MAX_CHUNK 0x1000UL

typedef struct _MB_PHYS_REQUEST {
    ULONG64 PhysicalAddress;
    ULONG   Size;
    ULONG   Reserved;
} MB_PHYS_REQUEST, *PMB_PHYS_REQUEST;

static
NTSTATUS
MbReadPhysical(
    _In_ PVOID OutputBuffer,
    _In_ ULONG OutputLength,
    _In_ const MB_PHYS_REQUEST *Request
    )
{
    ULONG done = 0;
    ULONG size = Request->Size;

    if (size == 0 || OutputLength < size) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    while (done < size) {
        ULONG chunk = size - done;
        PHYSICAL_ADDRESS pa;
        PVOID mapped;

        if (chunk > MB_MAX_CHUNK) {
            chunk = MB_MAX_CHUNK;
        }
        pa.QuadPart = (LONGLONG)(Request->PhysicalAddress + done);
        mapped = MmMapIoSpace(pa, chunk, MmNonCached);
        if (mapped == NULL) {
            return STATUS_UNSUCCESSFUL;
        }
        RtlCopyMemory((PUCHAR)OutputBuffer + done, mapped, chunk);
        MmUnmapIoSpace(mapped, chunk);
        done += chunk;
    }
    return STATUS_SUCCESS;
}

static
NTSTATUS
MbWritePhysical(
    _In_ PVOID InputBuffer,
    _In_ ULONG InputLength,
    _In_ const MB_PHYS_REQUEST *Request
    )
{
    ULONG done = 0;
    ULONG size = Request->Size;
    PUCHAR data = (PUCHAR)InputBuffer + sizeof(MB_PHYS_REQUEST);

    if (size == 0 || InputLength < sizeof(MB_PHYS_REQUEST) + size) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    while (done < size) {
        ULONG chunk = size - done;
        PHYSICAL_ADDRESS pa;
        PVOID mapped;

        if (chunk > MB_MAX_CHUNK) {
            chunk = MB_MAX_CHUNK;
        }
        pa.QuadPart = (LONGLONG)(Request->PhysicalAddress + done);
        mapped = MmMapIoSpace(pa, chunk, MmNonCached);
        if (mapped == NULL) {
            return STATUS_UNSUCCESSFUL;
        }
        RtlCopyMemory(mapped, data + done, chunk);
        MmUnmapIoSpace(mapped, chunk);
        done += chunk;
    }
    return STATUS_SUCCESS;
}

static
NTSTATUS
MbDispatchCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static
NTSTATUS
MbDispatchIoctl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
    )
{
    PIO_STACK_LOCATION stack;
    PVOID buffer;
    ULONG inputLength;
    ULONG outputLength;
    ULONG code;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG information = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    stack = IoGetCurrentIrpStackLocation(Irp);
    buffer = Irp->AssociatedIrp.SystemBuffer;
    inputLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    code = stack->Parameters.DeviceIoControl.IoControlCode;

    if (KeGetCurrentIrql() != PASSIVE_LEVEL) {
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto complete;
    }

    switch (code) {
    case IOCTL_MB_READ:
        if (buffer != NULL && inputLength >= sizeof(MB_PHYS_REQUEST)) {
            status = MbReadPhysical(buffer, outputLength, (PMB_PHYS_REQUEST)buffer);
            if (NT_SUCCESS(status)) {
                information = ((PMB_PHYS_REQUEST)buffer)->Size;
            }
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_MB_WRITE:
        if (buffer != NULL && inputLength >= sizeof(MB_PHYS_REQUEST)) {
            status = MbWritePhysical(buffer, inputLength, (PMB_PHYS_REQUEST)buffer);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_MB_QUERY:
        if (buffer != NULL && outputLength >= sizeof(ULONG)) {
            *(PULONG)buffer = 1;
            information = sizeof(ULONG);
            status = STATUS_SUCCESS;
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

complete:
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

static
VOID
MbDriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
    )
{
    UNICODE_STRING dosName;
    RtlInitUnicodeString(&dosName, MEMBRIDGE_PHYS_DOS_NAME);
    IoDeleteSymbolicLink(&dosName);
    if (DriverObject->DeviceObject != NULL) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING dosName;
    PDEVICE_OBJECT deviceObject = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    RtlInitUnicodeString(&deviceName, MEMBRIDGE_PHYS_DEVICE_NAME);
    RtlInitUnicodeString(&dosName, MEMBRIDGE_PHYS_DOS_NAME);

    status = IoCreateDevice(DriverObject, 0, &deviceName,
                            FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = IoCreateSymbolicLink(&dosName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = MbDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = MbDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MbDispatchIoctl;
    DriverObject->DriverUnload = MbDriverUnload;

    deviceObject->Flags |= DO_BUFFERED_IO;
    return STATUS_SUCCESS;
}
