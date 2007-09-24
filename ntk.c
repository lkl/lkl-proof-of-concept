#include <ddk/ntddk.h>
#undef FASTCALL
#include <linux/stat.h>
#include <asm/callbacks.h>
#include <asm/unistd.h>
#undef FASTCALL

#define assert(x) if (!(x)) { DbgPrint("Assert failed at %s:%d\n", __FILE__, __LINE__); while (1); }

int lkl_disk_add_disk(const char *filename, int which, dev_t *devno);
extern int snprintf(char * buf, size_t size, const char * fmt, ...)
	__attribute__ ((format (printf, 3, 4)));
extern int sprintf(char * buf, const char * fmt, ...)
	__attribute__ ((format (printf, 2, 3)));


struct _thread_info {
        HANDLE th;
        KSEMAPHORE sched_sem;
	int dead;
};

struct kernel_thread_helper_arg {
        int (*fn)(void*);
        void *arg;
        struct _thread_info *pti;
};

int linux_thread_info_size=sizeof(struct _thread_info);

void* linux_thread_info_alloc(void)
{
        struct _thread_info *pti=ExAllocatePool(PagedPool, sizeof(*pti));

	if (!pti)
		KeBugCheck(0);

        KeInitializeSemaphore(&pti->sched_sem, 0, 100);
	pti->dead=0;

	return pti;
}

static int debug_thread_count;

void linux_context_switch(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;

        KeReleaseSemaphore(&_next->sched_sem, 0, 1, 0);
        KeWaitForSingleObject(&_prev->sched_sem, Executive, KernelMode, FALSE, NULL);
	if (_prev->dead) {
		ExFreePool(_prev);
		debug_thread_count--;
		PsTerminateSystemThread(0);
	}
	
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
        struct _thread_info *pti=(struct _thread_info*)arg;

	pti->dead=1;
	KeReleaseSemaphore(&pti->sched_sem, 0, 1, 0);
}

int linux_new_thread(int (*fn)(void*), void *arg, void *pti)
{
        struct kernel_thread_helper_arg ktha = {
                .fn = fn,
                .arg = arg,
                .pti = (struct _thread_info*)pti
        };

	debug_thread_count++;
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
        _phys_mem=ExAllocatePool(PagedPool, *phys_mem_size);
	*phys_mem=(unsigned long)_phys_mem;
	
}

void mount_file(const char *filename, const char *fs)
{
	dev_t dev;
	char dev_str[]= { "/dev/xxxxxxxxxxxxxxxx" };
	char *mnt;

	assert(lkl_disk_add_disk(filename, 0, &dev) == 0);

	/* create /dev/dev */
	snprintf(dev_str, sizeof(dev_str), "/dev/%016x", dev);
	sys_unlink(dev_str);
	assert(sys_mknod(dev_str, S_IFBLK|0600, dev) == 0);

	/* create /mnt/filename */ 
	assert(sys_mkdir("/mnt", 0700) == 0);
	mnt=ExAllocatePool(PagedPool, strlen("/mnt/")+strlen(filename)+1);
	sprintf(mnt, "/mnt/%s", filename);
	assert(sys_mkdir(mnt, 0700) == 0);

	/* mount and chdir */
	assert(sys_safe_mount(dev_str, mnt, (char*)fs, 0, 0) == 0);
	assert(sys_chdir(mnt) == 0);
	
	ExFreePool(mnt);
}

void linux_main(void)
{
	struct timespec ts = { .tv_sec = 1};

	//mount_file("\\DosDevices\\PhysicalDrive1", "ext3");
	mount_file("\\Device\\FileDisk\\FileDisk0", "ext3");
	

	int fd=sys_open(".", O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0), i;
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

	for(i=3; i>0; i--) {
		DbgPrint("Shutdown in %d", i); 
		sys_nanosleep(&ts, NULL);
	}
}

KSEMAPHORE idle_sem;

void linux_exit_idle(void)
{
	KeReleaseSemaphore(&idle_sem, 0, 1, 0);
}

void linux_enter_idle(int halted)
{
	LARGE_INTEGER li = {
		.QuadPart = 0,
	};

	KeWaitForSingleObject(&idle_sem, Executive, KernelMode, FALSE, halted?&li:NULL);
}

/*
 * With 64 bits, we can cover about 584 years at a nanosecond resolution. 
 * Windows counts time from 1601 (do they plan to send a computer back in time
 * and take over the world??) so we neeed to do some substractions, otherwise we
 * would overflow. 
 */
LARGE_INTEGER basetime;

unsigned long long linux_time(void)
{
	LARGE_INTEGER li;

	KeQuerySystemTime(&li);
	
        return (li.QuadPart-basetime.QuadPart)*100;
}

static KDPC timer_dpc;

void linux_timer(unsigned long delta);

static void DDKAPI timer_dpc_f(KDPC *dpc, void *arg, void *x, void *y)
{
	//FIXME: need to disable at least dpc while running irqs so that the
	//enqueue and dequeue operations of the irq queue don't race; we
	//probably need to introduce new irq disable/enable ops 
	linux_trigger_irq(TIMER_IRQ);
}

KTIMER timer;

void linux_timer(unsigned long delta)
{
	if (!delta)
		KeCancelTimer(&timer);
	else
		KeSetTimer(&timer, RtlConvertLongToLargeInteger((unsigned long)(-(delta/100))), &timer_dpc);
}


void linux_halt(void)
{
	ExFreePool(_phys_mem);
}


static struct linux_native_operations lnops = {
	.panic_blink = linux_panic_blink,
	.mem_init = linux_mem_init,
	.main = linux_main,
	.halt = linux_halt,
	.thread_info_alloc = linux_thread_info_alloc,
	.new_thread = linux_new_thread,
	.free_thread = linux_free_thread,
	.context_switch = linux_context_switch,
	.enter_idle = linux_enter_idle,
	.exit_idle = linux_exit_idle,
	.timer = linux_timer,
	.time = linux_time,
};

void DDKAPI DriverUnload(PDRIVER_OBJECT driver)
{
	//FIXME: make this processor friendly
	while (debug_thread_count != 0)
		;
	DbgPrint("driver unload");
	return;
}

HANDLE lith;

void DDKAPI linux_idle_thread(void *arg)
{
	linux_start_kernel(&lnops, "");
}

NTSTATUS DDKAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
	driver->DriverUnload = DriverUnload;

        KeInitializeSemaphore(&kth_sem, 0, 100);	

	KeInitializeSemaphore(&idle_sem, 0, 100);	

	KeInitializeDpc(&timer_dpc, timer_dpc_f, NULL);
	KeInitializeTimer(&timer);
	
	KeQuerySystemTime(&basetime);


	PsCreateSystemThread(&lith, THREAD_ALL_ACCESS, NULL, NULL, NULL,
			     linux_idle_thread, NULL);

	return STATUS_SUCCESS;  
}
