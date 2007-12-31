#define _NO_OLDNAMES
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>

#undef __GLIBC__
#include <linux/stat.h>
#include <asm/unistd.h>
#include <asm/callbacks.h>
#include <asm/disk.h>
#include <linux/autoconf.h>
#include <asm/env.h>

static dev_t dev;
FILE *f;
int file_disk_add_disk(const char *filename, int which, dev_t *devno);

#ifndef CONFIG_LKL_ENV_APR
void* file_open(const char *name)
{
	return fopen(name, "r+b");
}

void file_close(void *f)
{
	fclose(f);
}
#else
extern void* file_open(const char *);
extern void file_close(void *);
extern void apr_init(void);
#endif

int init(void)
{
	f=file_open("disk");
	dev=lkl_disk_add_disk(f, "disk", 0, 10000);
	if (dev != 0)
		return 0;
	return -1;
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
	assert(lkl_sys_chroot(".") == 0);
}

void umount_disk(void)
{
	lkl_sys_umount("/", 0);
}

int main(int argc, char **argv, char **env)
{
#ifdef CONFIG_LKL_ENV_APR
	apr_init();
#endif
	lkl_env_init(init, 16*1024*1024);

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

	umount_disk();

	lkl_sys_halt();

	file_close(f);

        return 0;
}
