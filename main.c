#define _NO_OLDNAMES
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>

#include <asm/unistd.h>
#include <asm/callbacks.h>

#include "drivers/linux/file_disk-major.h"

static struct linux_native_operations lnops;

void linux_main(void)
{
	int fd=sys_open("/", O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0), i;
	if (fd >= 0) {
		char x[4096];
		int count, reclen;
		struct linux_dirent *de;

		count=sys_getdents(fd, (struct linux_dirent*)x, sizeof(x));
		assert(count>0);

		de=(struct linux_dirent*)x;
		while (count > 0) {
			reclen=de->d_reclen;
			printf("%s %ld\n", de->d_name, de->d_ino);
			de=(struct linux_dirent*)((char*)de+reclen); count-=reclen;
		}

		sys_close(fd);
	}

	/* test timers */
	if (lnops.timer) {
		struct timespec ts = { .tv_sec = 1};
		for(i=3; i>0; i--) {
			printf("Shutdown in %d \r", i); fflush(stdout);
			sys_nanosleep(&ts, NULL);
		}
	}


}

long linux_panic_blink(long time)
{
        assert(0);
        return 0;
}

static void *_phys_mem;

void linux_mem_init(unsigned long *phys_mem, unsigned long *phys_mem_size)
{
        *phys_mem_size=256*1024*1024;
        *phys_mem=(unsigned long)malloc(*phys_mem_size);
}

void linux_halt(void)
{
	free(_phys_mem);
}

extern void threads_init(struct linux_native_operations *lnops);

static struct linux_native_operations lnops = {
	.panic_blink = linux_panic_blink,
	.mem_init = linux_mem_init,
	.main = linux_main,
	.halt = linux_halt
};

int main(void)
{
	threads_init(&lnops);
        linux_start_kernel(&lnops, "root=%d:0", FILE_DISK_MAJOR);
        return 0;
}
