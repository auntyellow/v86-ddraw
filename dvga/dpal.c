#include <ntddk.h>

#ifdef NDEBUG
  #define printf(x)
#else
  #define printf(x) DbgPrint x
#endif

#define DEVICE_NAME  L"\\Device\\DirectPalette"
#define SYMLINK_NAME L"\\DosDevices\\DirectPalette"

NTSTATUS Stub(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
  printf(("Stub: %u\n", stack->MajorFunction));
  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

NTSTATUS DirectWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
  PUCHAR buffer = (PUCHAR) Irp->AssociatedIrp.SystemBuffer;
  ULONG n = stack->Parameters.Write.Length/4;
  ULONG i;
  if (n > 256) {
    n = 256;
  }
  printf(("Ready to write: %u bytes\n", n*4));
  __try {
    for (i = 0; i < n; i ++) {
      WRITE_PORT_UCHAR((PUCHAR) 0x3c8, (UCHAR) i);
      WRITE_PORT_UCHAR((PUCHAR) 0x3c9, (UCHAR) (buffer[0]>>2));
      WRITE_PORT_UCHAR((PUCHAR) 0x3c9, (UCHAR) (buffer[1]>>2));
      WRITE_PORT_UCHAR((PUCHAR) 0x3c9, (UCHAR) (buffer[2]>>2));
      buffer += 4;
    }
    printf(("WRITE_PORT_UCHAR Done\n"));
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = n*4;
  } __except (
    DbgPrint("Exception Addr: 0x%p\n", ((PEXCEPTION_POINTERS) GetExceptionInformation())->ExceptionRecord->ExceptionAddress),
    EXCEPTION_EXECUTE_HANDLER
  ) {
    printf(("WRITE_PORT_UCHAR Exception. Current IRQL: %u, Exception Code: 0x%08X\n", KeGetCurrentIrql(), GetExceptionCode()));
    Irp->IoStatus.Status = GetExceptionCode();
    Irp->IoStatus.Information = 0;
  }

  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return Irp->IoStatus.Status;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
  UNICODE_STRING symlinkName;
  RtlInitUnicodeString(&symlinkName, SYMLINK_NAME);
  IoDeleteSymbolicLink(&symlinkName);
  IoDeleteDevice(DriverObject->DeviceObject);
  printf(("Driver Unloaded\n"));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
  ULONG i;
  UNICODE_STRING deviceName;
  UNICODE_STRING symlinkName;
  PDEVICE_OBJECT deviceObject;
  NTSTATUS status;

  RtlInitUnicodeString(&deviceName, DEVICE_NAME);
  RtlInitUnicodeString(&symlinkName, SYMLINK_NAME);
  printf(("DriverEntry\n"));
  status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
  if (!NT_SUCCESS(status)) {
    printf(("Failed to create device object, status = %x\n", status));
    return status;
  }
  deviceObject->Flags |= DO_BUFFERED_IO;

  status = IoCreateSymbolicLink(&symlinkName, &deviceName);
  if (!NT_SUCCESS(status)) {
    printf(("Failed to create symbolic link, status = %x\n", status));
    IoDeleteDevice(deviceObject);
    return status;
  }

  for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
    DriverObject->MajorFunction[i] = Stub;
  }
  DriverObject->MajorFunction[IRP_MJ_WRITE] = DirectWrite;
  DriverObject->DriverUnload = DriverUnload;
  deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

  return STATUS_SUCCESS;
}
