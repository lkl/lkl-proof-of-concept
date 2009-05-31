#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>

#include <asm/lkl.h>
#include <asm/disk.h>
#include <linux/autoconf.h>
#include <asm/env.h>


#define assert(x) if (!(x)) { printf("assert failed: %s:%d: %s\n", __FILE__, __LINE__, #x); exit(0); }

static __kernel_dev_t dev;
void *f;

#ifdef CONFIG_LKL_ENV_APR
#include <apr.h>
#include <apr_file_io.h>
#include <assert.h>

apr_pool_t *root_pool;

void apr_init(void)
{
	int rc;

	apr_app_initialize(NULL, NULL, NULL);

	rc = apr_pool_create(&root_pool, NULL);
	assert(rc == APR_SUCCESS);
}

void* file_open(const char *name)
{
	apr_file_t *file;
	int rc;

	rc=apr_file_open(&file, name, 
			 APR_FOPEN_READ| APR_FOPEN_WRITE|
			 APR_FOPEN_BINARY, APR_OS_DEFAULT,
			 root_pool);
	assert(rc == APR_SUCCESS);
	return file;
}

void file_close(void *f)
{
	apr_file_close(f);
}
#else /* CONFIG_LKL_ENV_APR */
void* file_open(const char *name)
{
	return fopen(name, "r+b");
}

void file_close(void *f)
{
	fclose(f);
}
#endif  /* CONFIG_LKL_ENV_APR */


void mount_disk(const char *filename, const char *fs)
{
	char dev_str[]= { "/dev/xxxxxxxxxxxxxxxx" };

	f=file_open("disk");
	assert(f != NULL);
	dev=lkl_disk_add_disk(f, 20480000);
	assert(dev != 0);

	/* create /dev/dev */
	snprintf(dev_str, sizeof(dev_str), "/dev/%016x", dev);
	lkl_sys_unlink(dev_str);
	assert(lkl_sys_mknod(dev_str, S_IFBLK|0600, dev) == 0);

	/* mount and chdir */
	assert(lkl_sys_mount(dev_str, "/root", (char*)fs, 0, 0) == 0);
	assert(lkl_sys_chdir("/root") == 0);
	assert(lkl_sys_chroot(".") == 0);
}

void umount_disk(void)
{
	lkl_sys_umount("/", 0);
}

int main(int argc, char **argv, char **env)
{
	int fd, i, tmp;
#ifdef CONFIG_LKL_ENV_APR
	apr_init();
#endif
	lkl_env_init(16*1024*1024);

	mount_disk("disk", "ext3");


	fd=lkl_sys_open("CREDITS", O_RDONLY, 0);
	char buffer[5098];
	while ((tmp=lkl_sys_read(fd, buffer, sizeof(buffer))) > 0) {
		write(1, buffer, tmp);
	}
	lkl_sys_close(fd);

	fd=lkl_sys_open(".", O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0), i;
	if (fd >= 0) {
		char x[4096];
		int count, reclen;
		struct __kernel_dirent *de;

		count=lkl_sys_getdents(fd, (struct __kernel_dirent*)x, sizeof(x));
		assert(count>0);

		de=(struct __kernel_dirent*)x;
		while (count > 0) {
			reclen=de->d_reclen;
			printf("%s %ld\n", de->d_name, de->d_ino);
			de=(struct __kernel_dirent*)((char*)de+reclen); count-=reclen;
		}

		lkl_sys_close(fd);
	}


	/* test timers */
	struct __kernel_timespec ts = { .tv_sec = 1};
	for(i=3; i>0; i--) {
		printf("Shutdown in %d \r", i); fflush(stdout);
		lkl_sys_nanosleep(&ts, NULL);
	}

	umount_disk();

	lkl_sys_halt();

	file_close(f);

        return 0;
}
