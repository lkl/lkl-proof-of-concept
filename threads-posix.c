#include <pthread.h>
#include <malloc.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#include <asm/callbacks.h>

struct _thread_info {
        pthread_t th;
        pthread_mutex_t sched_mutex;
};

struct kernel_thread_helper_arg {
        int (*fn)(void*);
        void *arg;
        struct _thread_info *pti;
};

void linux_thread_info_init(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

        pthread_mutex_init(&pti->sched_mutex, NULL);
        pthread_mutex_lock(&pti->sched_mutex);
}

void linux_context_switch(void *prev, void *next)
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

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
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

void* linux_new_sem(int count)
{
	pthread_sem_t *sem=malloc(sizeof(*sem));

	if (!sem)
		return NULL;

	pthread_mutex_init(&sem->lock, NULL);
	pthread_cond_init(&sem->cond, NULL);
	sem->count=count;

	return sem;
}

int signals=0;

void linux_sem_up(void *_sem)
{
	pthread_sem_t *sem=(pthread_sem_t*)_sem;

	/*
	 * Signals and pthread don't mix well. For now, just do busy waiting, so
	 * that we can test the timer code.
	 */
	if (signals)
		return;

	pthread_mutex_lock(&sem->lock);
	sem->count++;
	if (sem->count > 0)
		pthread_cond_signal(&sem->cond);
	pthread_mutex_unlock(&sem->lock);
}

void linux_sem_down(void *_sem)
{
	pthread_sem_t *sem=(pthread_sem_t*)_sem;

	/*
	 * Signals and pthread don't mix well. For now, just do busy waiting, so
	 * that we can test the timer code.
	 */
	if (signals)
		return;
	
	pthread_mutex_lock(&sem->lock);
	if (sem->count <= 0)
		pthread_cond_wait(&sem->cond, &sem->lock);
	pthread_mutex_unlock(&sem->lock);
}

unsigned long long linux_time(void)
{
        struct timeval tv;

        gettimeofday(&tv, NULL);

        return tv.tv_sec*1000000000ULL+tv.tv_usec*1000ULL;
}

void sigalrm(int sig)
{
        linux_trigger_irq(TIMER_IRQ);
}

void linux_timer(unsigned long delta)
{
        unsigned long long delta_us=delta/1000;
        struct timeval tv = {
                .tv_sec = delta_us/1000000,
                .tv_usec = delta_us%1000000
        };
        struct itimerval itval = {
                .it_interval = {0, },
                .it_value = tv
        };
        
        setitimer(ITIMER_REAL, &itval, NULL);
}

void threads_init(struct linux_native_operations *lnops)
{
	lnops->thread_info_size=sizeof(struct _thread_info);
	lnops->thread_info_init=linux_thread_info_init;
	lnops->new_thread=linux_new_thread;
	lnops->free_thread=linux_free_thread;
	lnops->context_switch=linux_context_switch;

	lnops->new_sem=linux_new_sem;
	lnops->sem_down=linux_sem_down;
	lnops->sem_up=linux_sem_up;

	signals=1;
        lnops->time=linux_time;
        signal(SIGALRM, sigalrm);
        lnops->timer=linux_timer;

        pthread_mutex_lock(&kth_mutex);
}

