#include <pthread.h>
#include <malloc.h>

struct _thread_info {
        pthread_t th;
        pthread_mutex_t sched_mutex;
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

        pthread_mutex_init(&pti->sched_mutex, NULL);
        pthread_mutex_lock(&pti->sched_mutex);
}

void linux_switch_to(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;
        
        pthread_mutex_unlock(&_next->sched_mutex);
        pthread_mutex_lock(&_prev->sched_mutex);
}

pthread_mutex_t kth_mutex = PTHREAD_MUTEX_INITIALIZER;

void* kernel_thread_helper(void *arg)
{
        struct kernel_thread_helper_arg *ktha=(struct kernel_thread_helper_arg*)arg;
        int (*fn)(void*)=ktha->fn;
        void *farg=ktha->arg;
        struct _thread_info *pti=ktha->pti;

        pthread_mutex_unlock(&kth_mutex);
        pthread_mutex_lock(&pti->sched_mutex);
        return (void*)fn(farg);
}

void linux_free_thread(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

        pthread_cancel(pti->th);
}

int linux_new_thread(int (*fn)(void*), void *arg, void *pti)
{
        struct kernel_thread_helper_arg ktha = {
                .fn = fn,
                .arg = arg,
                .pti = (struct _thread_info*)pti
        };
        int ret;


        ret=pthread_create(&ktha.pti->th, NULL, kernel_thread_helper, &ktha);
        pthread_mutex_lock(&kth_mutex);
        return ret;
}

typedef struct {
	pthread_mutex_t lock;
	int count;
	pthread_cond_t cond;
} pthread_sem_t;

void* linux_sem_new(int count)
{
	pthread_sem_t *sem=malloc(sizeof(*sem));

	if (!sem)
		return NULL;

	pthread_mutex_init(&sem->lock, NULL);
	pthread_cond_init(&sem->cond, NULL);
	sem->count=count;

	return sem;
}

void linux_sem_up(void *_sem)
{
	pthread_sem_t *sem=(pthread_sem_t*)_sem;

	pthread_mutex_lock(&sem->lock);
	sem->count++;
	if (sem->count > 0)
		pthread_cond_signal(&sem->cond);
	pthread_mutex_unlock(&sem->lock);
}

void linux_sem_down(void *_sem)
{
	pthread_sem_t *sem=(pthread_sem_t*)_sem;

	pthread_mutex_lock(&sem->lock);
	if (sem->count <= 0)
		pthread_cond_wait(&sem->cond, &sem->lock);
	pthread_mutex_unlock(&sem->lock);
}


void threads_init(void)
{
        pthread_mutex_lock(&kth_mutex);
}

