#define _NO_OLDNAMES
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>


#undef __GLIBC__
#include <linux/stat.h>
#include <asm/unistd.h>
#include <asm/callbacks.h>

static struct linux_native_operations lnops;

static long linux_panic_blink(long time)
{
        assert(0);
        return 0;
}

static void *_phys_mem;

static void linux_mem_init(unsigned long *phys_mem, unsigned long *phys_mem_size)
{
        *phys_mem_size=256*1024*1024;
        *phys_mem=(unsigned long)malloc(*phys_mem_size);
}

static void linux_halt(void)
{
	free(_phys_mem);
}

extern void threads_init(struct linux_native_operations *lnops, int (*init)(void));

static struct linux_native_operations lnops = {
	.panic_blink = linux_panic_blink,
	.mem_init = linux_mem_init,
	.halt = linux_halt
};


static dev_t dev;

int file_disk_add_disk(const char *filename, int which, dev_t *devno);

int init(void)
{
	return file_disk_add_disk("disk", 0, &dev);
}

void mount_disk(const char *filename, const char *fs)
{
	char dev_str[]= { "/dev/xxxxxxxxxxxxxxxx" };
	char *mnt;

	/* create /dev/dev */
	snprintf(dev_str, sizeof(dev_str), "/dev/%016x", dev);
	lkl_sys_unlink(dev_str);
	assert(lkl_sys_mknod(dev_str, S_IFBLK|0600, dev) == 0);

	/* create /mnt/filename */ 
	assert(lkl_sys_mkdir("/mnt", 0700) == 0);
	mnt=malloc(strlen("/mnt/")+strlen(filename)+1);
	sprintf(mnt, "/mnt/%s", filename);
	assert(lkl_sys_mkdir(mnt, 0700) == 0);

	/* mount and chdir */
	assert(lkl_sys_mount(dev_str, mnt, (char*)fs, 0, 0) == 0);
	assert(lkl_sys_chdir(mnt) == 0);
}

int main(void)
{
	threads_init(&lnops, init); 

	mount_disk("disk", "ext3");
	
	int fd=lkl_sys_open(".", O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0), i;
	if (fd >= 0) {
		char x[4096];
		int count, reclen;
		struct linux_dirent *de;

		count=lkl_sys_getdents(fd, (struct linux_dirent*)x, sizeof(x));
		assert(count>0);

		de=(struct linux_dirent*)x;
		while (count > 0) {
			reclen=de->d_reclen;
			printf("%s %ld\n", de->d_name, de->d_ino);
			de=(struct linux_dirent*)((char*)de+reclen); count-=reclen;
		}

		lkl_sys_close(fd);
	}

	/* test timers */
	struct timespec ts = { .tv_sec = 1};
	for(i=3; i>0; i--) {
		printf("Shutdown in %d \r", i); fflush(stdout);
		lkl_sys_nanosleep(&ts, NULL);
	}


	lkl_sys_halt();

        return 0;
}
