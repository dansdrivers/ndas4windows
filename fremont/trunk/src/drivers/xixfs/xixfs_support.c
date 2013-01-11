#include <ntddk.h>


NTSTATUS
Xixfs_GenerateUuid(OUT void * uuid)
{
	return ExUuidCreate((UUID *)uuid);
}




