#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>

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
        HANDLE th;
        HANDLE sched_sem;
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

        pti->sched_sem=CreateSemaphore(NULL, 0, 100, NULL);
}

void _switch_to(void *prev, void *next)
{
        struct _thread_info *_prev=(struct _thread_info*)prev;
        struct _thread_info *_next=(struct _thread_info*)next;
        
        ReleaseSemaphore(_next->sched_sem, 1, NULL);
        WaitForSingleObject(_prev->sched_sem, INFINITE);
}

HANDLE kth_sem;

void* kernel_thread_helper(void *arg)
{
        struct kernel_thread_helper_arg *ktha=(struct kernel_thread_helper_arg*)arg;
        int (*fn)(void*)=ktha->fn;
        void *farg=ktha->arg;
        struct _thread_info *pti=ktha->pti;

        ReleaseSemaphore(kth_sem, 1, NULL);
        WaitForSingleObject(pti->sched_sem, INFINITE);
        return (void*)fn(farg);
}

extern void* current_private_thread_info(void);

void destroy_thread(void *arg)
{
        struct _thread_info *pti=(struct _thread_info*)arg;

        TerminateThread(pti->th, 0);
}

int _copy_thread(int (*fn)(void*), void *arg, void *pti)
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
	#define O_RDONLY 0
	#define O_LARGEFILE 00100000
	#define O_DIRECTORY 00200000
        if (strcmp(filename, "/bin/init") == 0) {
                int fd=sys_open("/", O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0);
                if (fd >= 0) {
                        char x[4096];
                        int count, reclen;
			#define NAME_MAX 255
                        struct dirent {

				long d_ino;                 /* inode number */
				off_t d_off;                /* offset to next dirent */
				unsigned short d_reclen;    /* length of this dirent */
				char d_name [NAME_MAX+1];   /* filename (null-terminated) */
			} *de;

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

        return 0; 

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
        *phys_mem=malloc(*phys_mem_size);
}

int main(void)
{
        kth_sem=CreateSemaphore(NULL, 0, 100, NULL);
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

int do_read(int fd, char *buffer, int size)
{
	int n, from=0;
	
	while (1) {
		n=read(fd, &buffer[from], size-from);
		if (n <= 0)
		        return -1;
	        if (n+from == size)
	    	        return 0;
		from+=n;
	}
}

int do_write(int fd, char *buffer, int size)
{
	int n, from=0;
	
	while (1) {
		n=write(fd, &buffer[from], size-from);
		if (n <= 0)
		        return -1;
	        if (n+from == size)
	    	        return 0;
		from+=n;
	}
}


void _sbull_transfer(int fd, unsigned long sector, unsigned long nsect, char *buffer, int dir)
{
	int x;
        x=lseek64(fd, sector*512, SEEK_SET);
	assert(x >= 0);
        if (dir)
                x=do_write(fd, buffer, nsect*512);
        else
                x=do_read(fd, buffer, nsect*512);
	assert(x == 0);

}

