#include <ddk/ntddk.h>

#include <asm/callbacks.h>
#include <asm/unistd.h>

struct _thread_info {
        HANDLE th;
        KSEMAPHORE sched_sem;
};

struct kernel_thread_helper_arg {
        int (*fn)(void*);
        void *arg;
        struct _thread_info *pti;
};

int linux_thread_info_size=sizeof(struct _thread_info);

void linux_thread_info_init(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

        KeInitializeSemaphore(&pti->sched_sem, 0, 100);
}

void linux_context_switch(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;
        
        KeReleaseSemaphore(&_next->sched_sem, 0, 1, 0);
        KeWaitForSingleObject(&_prev->sched_sem, Executive, KernelMode, FALSE, NULL);
}

KSEMAPHORE kth_sem;

void DDKAPI kernel_thread_helper(void *arg)
{
        struct kernel_thread_helper_arg *ktha=(struct kernel_thread_helper_arg*)arg;
        int (*fn)(void*)=ktha->fn;
        void *farg=ktha->arg;
        struct _thread_info *pti=ktha->pti;

        KeReleaseSemaphore(&kth_sem, 0, 1, 0);
        KeWaitForSingleObject(&pti->sched_sem, Executive, KernelMode, FALSE, NULL);
        fn(farg);
}


void linux_free_thread(void *arg)
{
	//no way we can terminate a thread from another context
	//after all, we do need a tiny kernel patch 
}

int linux_new_thread(int (*fn)(void*), void *arg, void *pti)
{
        struct kernel_thread_helper_arg ktha = {
                .fn = fn,
                .arg = arg,
                .pti = (struct _thread_info*)pti
        };


	PsCreateSystemThread(&ktha.pti->th, THREAD_ALL_ACCESS, NULL, NULL, NULL,
			     kernel_thread_helper, &ktha);
	KeWaitForSingleObject(&kth_sem, Executive, KernelMode, FALSE, NULL);
        return 0;
}

long linux_panic_blink(long time)
{
	while (1);
        return 0;
}

static void *_phys_mem;

void linux_mem_init(unsigned long *phys_mem, unsigned long *phys_mem_size)
{
        *phys_mem_size=64*1024*1024;
        *phys_mem=(unsigned long)ExAllocatePool(PagedPool, *phys_mem_size);
}

void linux_halt(void)
{
	ExFreePool(_phys_mem);
}

void linux_main(void)
{
	int fd=sys_open("/", O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0);
	if (fd >= 0) {
		static char x[4096];
		int count, reclen;
		struct linux_dirent *de;

		count=sys_getdents(fd, (struct linux_dirent*)x, sizeof(x));
		if (count <= 0)
			KeBugCheck(0);

		de=(struct linux_dirent*)x;
		while (count > 0) {
			reclen=de->d_reclen;
			DbgPrint("%s %ld\n", de->d_name, de->d_ino);
			de=(struct linux_dirent*)((char*)de+reclen); count-=reclen;
		}

		sys_close(fd);
	}
}

static struct linux_native_operations lnops = {
	.panic_blink = linux_panic_blink,
	.mem_init = linux_mem_init,
	.main = linux_main,
	.halt = linux_halt,
	.thread_info_size = sizeof(struct _thread_info),
	.thread_info_init = linux_thread_info_init,
	.new_thread = linux_new_thread,
	.free_thread = linux_free_thread,
	.context_switch = linux_context_switch

};

void DDKAPI DriverUnload(PDRIVER_OBJECT driver)
{
	DbgPrint("driver unload");
	return;
}

HANDLE lith;

void DDKAPI linux_idle_thread(void *arg)
{
	linux_start_kernel(&lnops, "root=%d:0", FILE_DISK_MAJOR);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
	DbgPrint("driver entry");
	driver->DriverUnload = DriverUnload;

        KeInitializeSemaphore(&kth_sem, 0, 100);	

	PsCreateSystemThread(&lith, THREAD_ALL_ACCESS, NULL, NULL, NULL,
			     linux_idle_thread, NULL);

	return STATUS_SUCCESS;  
}
