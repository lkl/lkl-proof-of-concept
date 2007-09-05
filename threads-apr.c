#include <apr_thread_mutex.h>
#include <apr_thread_proc.h>
#include <stdlib.h>

#include <asm/callbacks.h>


static apr_pool_t 		* lkl_thread_creator_pool;
static apr_thread_mutex_t	* kth_mutex;

struct _thread_info {
	apr_thread_t * th;
	apr_thread_mutex_t * sched_mutex;
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

	apr_thread_mutex_create(&pti->sched_mutex, APR_THREAD_MUTEX_DEFAULT, lkl_thread_creator_pool);
	apr_thread_mutex_lock(pti->sched_mutex);
}

void linux_switch_to(void *prev, void *next)
{
	struct _thread_info *_prev=(struct _thread_info*)prev;
	struct _thread_info *_next=(struct _thread_info*)next;

	apr_thread_mutex_unlock(_next->sched_mutex);
	apr_thread_mutex_lock(_prev->sched_mutex);
}



void* kernel_thread_helper(apr_thread_t * thd, void *arg)
{
	struct kernel_thread_helper_arg *ktha=(struct kernel_thread_helper_arg*)arg;
	int (*fn)(void*)=ktha->fn;
	void *farg=ktha->arg;
	struct _thread_info *pti=ktha->pti;

	apr_thread_mutex_unlock(kth_mutex);
	apr_thread_mutex_lock(pti->sched_mutex);
	return (void*)fn(farg);
}

void linux_free_thread(void *arg)
{
	struct _thread_info *pti=(struct _thread_info*)arg;

	apr_thread_exit(pti->th, 0);
}

int linux_new_thread(int (*fn)(void*), void *arg, void *pti)
{
	struct kernel_thread_helper_arg ktha = {
		.fn = fn,
		.arg = arg,
		.pti = (struct _thread_info*)pti
	};
	int ret;


	ret = apr_thread_create(&ktha.pti->th, NULL, kernel_thread_helper, &ktha, lkl_thread_creator_pool);
	apr_thread_mutex_lock(kth_mutex);
	return ret;
}

void threads_init(struct linux_native_operations *lnops)
{
	lnops->thread_info_size=sizeof(struct _thread_info);
	lnops->thread_info_init=linux_thread_info_init;
	lnops->new_thread=linux_new_thread;
	lnops->free_thread=linux_free_thread;
	lnops->switch_to=linux_switch_to;

	apr_initialize();
	atexit(apr_terminate);
	apr_pool_create(&lkl_thread_creator_pool, NULL);
	apr_thread_mutex_create(&kth_mutex, APR_THREAD_MUTEX_DEFAULT, lkl_thread_creator_pool);
	apr_thread_mutex_lock(kth_mutex);
}
