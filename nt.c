#include <windows.h>
#include <assert.h>

#include <asm/callbacks.h>

struct _thread_info {
        HANDLE th;
        HANDLE sched_sem;
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
        struct _thread_info *pti=malloc(sizeof(*pti));

	assert(pti != NULL);

        pti->sched_sem=CreateSemaphore(NULL, 0, 100, NULL);
	pti->dead=0;

	return pti;
}

int debug_thread_count;

void linux_context_switch(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;
        
        ReleaseSemaphore(_next->sched_sem, 1, NULL);
        WaitForSingleObject(_prev->sched_sem, INFINITE);
	if (_prev->dead) {
		CloseHandle(_prev->sched_sem);
		free(_prev);
		debug_thread_count--;
		ExitThread(0);
	}
		
}

HANDLE kth_sem;

DWORD WINAPI kernel_thread_helper(LPVOID arg)
{
        struct kernel_thread_helper_arg *ktha=(struct kernel_thread_helper_arg*)arg;
        int (*fn)(void*)=ktha->fn;
        void *farg=ktha->arg;
        struct _thread_info *pti=ktha->pti;

        ReleaseSemaphore(kth_sem, 1, NULL);
        WaitForSingleObject(pti->sched_sem, INFINITE);
        return fn(farg);
}

void linux_free_thread(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

	pti->dead=1;
	ReleaseSemaphore(pti->sched_sem, 1, NULL);
}

int linux_new_thread(int (*fn)(void*), void *arg, void *pti)
{
        struct kernel_thread_helper_arg ktha = {
                .fn = fn,
                .arg = arg,
                .pti = (struct _thread_info*)pti
        };

		
	debug_thread_count++;

        ktha.pti->th=CreateThread(NULL, 0, kernel_thread_helper, &ktha, 0, NULL);
	WaitForSingleObject(kth_sem, INFINITE);
        return 0;
}

HANDLE idle_sem;

void linux_exit_idle(void)
{
	ReleaseSemaphore(idle_sem, 1, NULL);
}

HANDLE timer;

void linux_enter_idle(int halted)
{
	HANDLE handles[]={idle_sem, timer};
	int count=sizeof(handles)/sizeof(HANDLE);
	int n;


	n=WaitForMultipleObjects(count, handles, FALSE, halted?0:INFINITE);
	
	assert(n < WAIT_OBJECT_0+count);

	n-=WAIT_OBJECT_0;

	if (n == 1) {
		linux_trigger_irq(TIMER_IRQ);
	}
	
	/* 
	 * It is OK to exit even if only the timer has expired, 
	 * as linux_trigger_irq will trigger an linux_exit_idle anyway 
	 */
}

/*
 * With 64 bits, we can cover about 584 years at a nanosecond resolution. 
 * Windows counts time from 1601 (do they plan to send a computer back in time
 * and take over the world??) so we neeed to do some substractions, otherwise we
 * would overflow. 
 */
static LARGE_INTEGER basetime;

unsigned long long linux_time(void)
{
	SYSTEMTIME st;
	FILETIME ft;
	LARGE_INTEGER li;

	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	

        return (li.QuadPart-basetime.QuadPart)*100;
}

void linux_timer(unsigned long delta)
{
	LARGE_INTEGER li = {
		.QuadPart = -((long)(delta/100)),
	};
        
	SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE);
}

static void (*main_halt)(void);

static void linux_halt(void)
{
	/* 
	 * It might take a while to terminate the threads because of the delay 
	 * induce by the sync termination procedure. Unfortunatelly there is no
	 * good way of waiting them.
	*/
	while (debug_thread_count != 0)
		Sleep(1);

	if (main_halt)
		main_halt();
}


HANDLE syscall_sem;
HANDLE syscall_sem_wait;

void* linux_syscall_prepare(void)
{
        WaitForSingleObject(&syscall_sem, INFINITE);
	return NULL;
}

void linux_syscall_wait(void *arg)
{
        WaitForSingleObject(syscall_sem_wait, INFINITE);
        ReleaseSemaphore(syscall_sem, 1, NULL);
}

void linux_syscall_done(void *arg)
{
        ReleaseSemaphore(syscall_sem_wait, 1, NULL);
}


DWORD WINAPI lkl_init_thread(LPVOID arg)
{
	struct linux_native_operations *lnops=(struct linux_native_operations*)arg;
	linux_start_kernel(lnops, "");
	return 0;
}

HANDLE lkl_init_sem;

static int (*main_init)(void);

int linux_init(void)
{
	int ret=main_init();
	ReleaseSemaphore(lkl_init_sem, 1, NULL);
	return ret;
}

void threads_init(struct linux_native_operations *lnops, int (*init)(void))
{
	SYSTEMTIME st;
	FILETIME ft;
	HANDLE init_thread;

	main_halt=lnops->halt;
	lnops->halt=linux_halt;
	main_init=init;

	lnops->thread_info_alloc=linux_thread_info_alloc;
	lnops->new_thread=linux_new_thread;
	lnops->free_thread=linux_free_thread;
	lnops->context_switch=linux_context_switch;
        kth_sem=CreateSemaphore(NULL, 0, 100, NULL);

	lnops->enter_idle=linux_enter_idle;
	lnops->exit_idle=linux_exit_idle;
        idle_sem=CreateSemaphore(NULL, 0, 100, NULL);

        lnops->time=linux_time;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	basetime.LowPart = ft.dwLowDateTime;
	basetime.HighPart = ft.dwHighDateTime;
        lnops->timer=linux_timer;
	timer=CreateWaitableTimer(NULL, FALSE, NULL);

        syscall_sem_wait=CreateSemaphore(NULL, 0, 100, NULL);
        syscall_sem=CreateSemaphore(NULL, 1, 100, NULL);
	lnops->syscall_prepare=linux_syscall_prepare;
	lnops->syscall_wait=linux_syscall_wait;
	lnops->syscall_done=linux_syscall_done;

	lkl_init_sem=CreateSemaphore(NULL, 0, 100, NULL);
	lnops->init=linux_init;
        init_thread=CreateThread(NULL, 0, lkl_init_thread, lnops, 0, NULL);
        WaitForSingleObject(lkl_init_sem, INFINITE);
}

