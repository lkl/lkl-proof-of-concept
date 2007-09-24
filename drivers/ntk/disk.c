#include <asm/callbacks.h>
#include <ddk/ntddk.h>
#include <ddk/ntdddisk.h>

#include "lkl/disk.h"

struct wdev {
	FILE_OBJECT *file;
	DEVICE_OBJECT *dev;
};

#define UNICODE_STRING_INIT(name, x) UNICODE_STRING name = { \
	.Buffer = L##x, \
	.Length = sizeof(L##x), \
	.MaximumLength = sizeof(L##x) \
}

UNICODE_STRING* INIT_UNICODE(UNICODE_STRING *ustr, const char *str) 
{
        ANSI_STRING astr;

        RtlInitAnsiString(&astr, str);
        if (RtlAnsiStringToUnicodeString(ustr, &astr, TRUE) != STATUS_SUCCESS) 
                return NULL;

        return ustr;
}

#define KeBugOn(x) if (x) { DbgPrint("bug %s:%d\n", __FUNCTION__, __LINE__); while (1); }

void* lkl_disk_do_open(const char *filename)
{
	NTSTATUS status;
	UNICODE_STRING ustr;
	struct wdev *dev;

	if (!(dev=ExAllocatePool(PagedPool, sizeof(*dev))))
		return NULL;

        INIT_UNICODE (&ustr, filename);
	    
	status = IoGetDeviceObjectPointer(&ustr, FILE_ALL_ACCESS,
					  &dev->file, &dev->dev);

	KeBugOn(status != STATUS_SUCCESS);

	return dev;
}

#define IOCTL_DISK_GET_LENGTH_INFO          CTL_CODE(IOCTL_DISK_BASE, 0x0017, METHOD_BUFFERED, FILE_READ_ACCESS) 

unsigned long lkl_disk_get_sectors(void *_dev)
{
	struct wdev *dev=(struct wdev*)_dev;
	GET_LENGTH_INFORMATION gli;
	IO_STATUS_BLOCK isb;
	NTSTATUS status;
	KEVENT event;
	IRP *irp;

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	irp=IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_LENGTH_INFO, dev->dev,
					  NULL, 0, &gli,
					  sizeof(gli), FALSE, &event, &isb);
	if (!irp) 
		return 0;

	if (IoCallDriver(dev->dev, irp) != STATUS_PENDING) {
		KeSetEvent(&event, 0, FALSE);
		isb.Status = status;
	}

	KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);

	if (isb.Status != STATUS_SUCCESS)
		return 0;
	
	return gli.Length.QuadPart/512;
}

#ifdef LKL_DISK_ASYNC
NTSTATUS lkl_disk_completion(DEVICE_OBJECT *dev, IRP *irp, void *arg)
{
	struct lkl_disk_cs *cs=(struct lkl_disk_cs*)arg;
	IO_STATUS_BLOCK *isb=irp->UserIosb;
	MDL *mdl;

	DbgPrint("%s:%d: %d\n", __FUNCTION__, __LINE__, isb->Status);

	if (isb->Status == STATUS_SUCCESS)
		cs->status=LKL_DISK_CS_SUCCESS;
	else
		cs->status=LKL_DISK_CS_ERROR;
	linux_trigger_irq_with_data(LKL_DISK_IRQ, cs);

	ExFreePool(isb);

	while ((mdl = irp->MdlAddress)) {
		DbgPrint("%s:%d\n", __FUNCTION__, __LINE__);
		irp->MdlAddress = mdl->Next;
		MmUnlockPages(mdl);
		IoFreeMdl(mdl);
	}

	if (irp->Flags & IRP_INPUT_OPERATION) {
		DbgPrint("%s:%d\n", __FUNCTION__, __LINE__);
		IO_STACK_LOCATION *isl = IoGetCurrentIrpStackLocation(irp);
		RtlCopyMemory(irp->UserBuffer, irp->AssociatedIrp.SystemBuffer, isl->Parameters.Read.Length);
	}

	if (irp->Flags & IRP_DEALLOCATE_BUFFER) {
		DbgPrint("%s:%d\n", __FUNCTION__, __LINE__);
		ExFreePoolWithTag(irp->AssociatedIrp.SystemBuffer, '  oI');
	}


	DbgPrint("%s:%d\n", __FUNCTION__, __LINE__);
	IoFreeIrp(irp);
	DbgPrint("%s:%d\n", __FUNCTION__, __LINE__);
	return STATUS_MORE_PROCESSING_REQUIRED;
}


void lkl_disk_do_rw(void *_dev, unsigned long sector, unsigned long nsect,
	      char *buffer, int dir, struct lkl_disk_cs *cs)

{
	struct wdev *dev=(struct wdev*)_dev;
	IRP *irp;
	IO_STATUS_BLOCK *isb;
	LARGE_INTEGER offset = {
		.QuadPart = sector*512,
	};

	DbgPrint("%s:%d: dir=%d buffer=%p offset=%u nsect=%u\n", __FUNCTION__, __LINE__, dir, buffer, sector, nsect);

	if (!(isb=ExAllocatePool(NonPagedPool, sizeof(*isb)))) {
		cs->status=LKL_DISK_CS_ERROR;
		linux_trigger_irq_with_data(LKL_DISK_IRQ, cs);
		return;
	}
		
	irp=IoBuildAsynchronousFsdRequest(dir?IRP_MJ_WRITE:IRP_MJ_READ,
					  dev->dev, buffer, 0 /*nsect*512*/, &offset,
					  isb);
	if (!irp) {
		ExFreePool(isb);
		cs->status=LKL_DISK_CS_ERROR;
		linux_trigger_irq_with_data(LKL_DISK_IRQ, cs);
		return;
	}

	IoSetCompletionRoutine(irp, lkl_disk_completion, cs, TRUE, TRUE, TRUE);

	IoCallDriver(dev->dev, irp);

}
#else
int lkl_disk_do_rw(void *_dev, unsigned long sector, unsigned long nsect,
		   char *buffer, int dir)
{
	struct wdev *dev=(struct wdev*)_dev;
	IRP *irp;
	IO_STATUS_BLOCK isb;
	KEVENT event;
	NTSTATUS status;
	LARGE_INTEGER offset = {
		.QuadPart = sector*512,
	};

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	irp=IoBuildSynchronousFsdRequest(dir?IRP_MJ_WRITE:IRP_MJ_READ,
					  dev->dev, buffer, nsect*512, &offset,
					  &event, &isb);
	if (!irp) 
		return LKL_DISK_CS_ERROR;

	if (IoCallDriver(dev->dev, irp) != STATUS_PENDING) {
		KeSetEvent(&event, 0, FALSE);
		isb.Status = status;
	}

	KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
	
	return (isb.Status == STATUS_SUCCESS)?LKL_DISK_CS_SUCCESS:LKL_DISK_CS_ERROR;
}
#endif
