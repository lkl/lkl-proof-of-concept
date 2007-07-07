#define _LARGEFILE64_SOURCE
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/sched.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/dirent.h>
#include <linux/unistd.h>

unsigned long irqs_enabled=0;

unsigned long __local_save_flags(void)
{
        return irqs_enabled;
}

void __local_irq_restore(unsigned long flags)
{
        irqs_enabled=flags;
}

void local_irq_enable(void)
{
        irqs_enabled=1;
}

void local_irq_disable(void)
{
        irqs_enabled=0;
}

void show_mem(void)
{
}

void __udelay(unsigned long usecs)
{

}

void __ndelay(unsigned long nsecs)
{

}


void lkl_console_write(const char *str, unsigned len)
{
        write(1, str, len);
}

void cpu_wait_events(void)
{
}

extern void start_kernel(void);

struct _thread_info {
        pthread_t th;
        pthread_mutex_t sched_mutex;
};

struct kernel_thread_helper_arg {
        int (*fn)(void*);
        void *arg;
        struct _thread_info *pti;
};

int private_thread_info_size(void)
{
        return sizeof(struct _thread_info);
}

void private_thread_info_init(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

        pthread_mutex_init(&pti->sched_mutex, NULL);
        pthread_mutex_lock(&pti->sched_mutex);
}

void _switch_to(void *prev, void *next)
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

extern void* current_private_thread_info(void);

void destroy_thread(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

        pthread_cancel(pti->th);
}

int _copy_thread(int (*fn)(void*), void *arg, void *pti)
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

extern int sbull_init(void);

int kernel_execve(const char *filename, char *const argv[], char *const envp[])
{
#if 0
        int i=1;

        printf("%s: %s ", __FUNCTION__, filename);
        while(argv[i]) {
                printf("%s ", argv[i]);
                fflush(stdout);
                i++;
        }
        printf("\n");

#endif

        if (strcmp(filename, "/bin/init") == 0) {
                int fd=sys_open("/", O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0);
                if (fd >= 0) {
                        char x[4096];
                        int count, reclen;
                        struct dirent *de;

                        count=sys_getdents(fd, x, sizeof(x));
                        assert(count>0);
                        printf("open: %d %d %d\n", count);

                        de=(struct dirent*)x;
                        while (count > 0) {
                                reclen=de->d_reclen;
                                printf("%s %ld\n", de->d_name, de->d_ino);
                                de=(struct dirent*)((char*)de+reclen); count-=reclen;
                        }

                        sys_close(fd);
                }
        }

        return -1; 

}


void get_cmd_line(char **cl)
{
        static char x[] = "root=42:0";
        *cl=x;
}

long _panic_blink(long time)
{
        assert(1);
        return 0;
}

void _mem_init(unsigned long *phys_mem, unsigned long *phys_mem_size)
{
        *phys_mem_size=256*1024*1024;
        *phys_mem=memalign(4096, *phys_mem_size);
}

int main(void)
{
        pthread_mutex_lock(&kth_mutex);
        start_kernel();
        return 0;
}

int _sbull_open(void)
{
        return open("disk", O_RDONLY);
}

unsigned long _sbull_sectors(void)
{
        unsigned long sectors;
        int fd=open("disk", O_RDONLY);


        assert(fd > 0);
        sectors=(lseek64(fd, 0, SEEK_END)/512);
        close(fd);

        return sectors;
}

void _sbull_transfer(int fd, unsigned long sector, unsigned long nsect, char *buffer, int dir)
{
        assert(lseek64(fd, sector*512, SEEK_SET) >= 0);
        
        if (dir)
                assert(write(fd, buffer, nsect*512) == nsect*512);
        else
                assert(read(fd, buffer, nsect*512) == nsect*512);
}
