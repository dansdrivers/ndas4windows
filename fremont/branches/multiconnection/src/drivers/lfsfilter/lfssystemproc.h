#ifndef __LFSSYSTEMPROC_H__
#define __LFSSYSTEMPROC_H__


NTSTATUS
LoadLfsSystem (
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
);


VOID
UnloadLfsSystem (
    IN PDRIVER_OBJECT  DriverObject
);





#endif