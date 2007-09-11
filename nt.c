#include <windows.h>

#include <asm/callbacks.h>

struct _thread_info {
        HANDLE th;
        HANDLE sched_sem;
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

        pti->sched_sem=CreateSemaphore(NULL, 0, 100, NULL);
}

void linux_context_switch(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;
        
        ReleaseSemaphore(_next->sched_sem, 1, NULL);
        WaitForSingleObject(_prev->sched_sem, INFINITE);
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

        TerminateThread(pti->th, 0);
}

int linux_new_thread(int (*fn)(void*), void *arg, void *pti)
{
        struct kernel_thread_helper_arg ktha = {
                .fn = fn,
                .arg = arg,
                .pti = (struct _thread_info*)pti
        };


        ktha.pti->th=CreateThread(NULL, 0, kernel_thread_helper, &ktha, 0, NULL);
	WaitForSingleObject(kth_sem, INFINITE);
        return 0;
}

/* only needed for async operations */
void* linux_sem_new(int count)
{
	return (void*)1;
}
void linux_sem_up(void *_sem)
{
}
void linux_sem_down(void *_sem)
{
}

void threads_init(struct linux_native_operations *lnops)
{
	lnops->thread_info_size=sizeof(struct _thread_info);
	lnops->thread_info_init=linux_thread_info_init;
	lnops->new_thread=linux_new_thread;
	lnops->free_thread=linux_free_thread;
	lnops->context_switch=linux_context_switch;

        kth_sem=CreateSemaphore(NULL, 0, 100, NULL);
}