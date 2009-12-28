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


void mount_disk(const char *filename, char *mntpath, size_t mntpath_size)
{
	int rc;
	f = file_open("disk");
	assert(f != NULL);
	dev = lkl_disk_add_disk(f, 20480000);
	assert(dev != 0);

	rc = lkl_mount_dev(dev, NULL, 0, NULL, mntpath, mntpath_size);
	if (rc)
		printf("mount_disk> lkl_mount_dev rc=%d\n", rc);
}

void umount_disk(void)
{
	int rc;
	rc = lkl_umount_dev(dev, 0);
	if (rc)
		printf("umount_disk> lkl_umount_dev rc=%d\n", rc);
	rc = lkl_disk_del_disk(dev);
	if (rc)
		printf("umount_disk> lkl_disk_del_disk rc=%d\n", rc);

	file_close(f);
}

void list_files(const char * path)
{
	int fd;
	printf("-------- printing contents of [%s]\n", path);
	fd = lkl_sys_open(path, O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0);
	if (fd >= 0) {
		char x[4096];
		int count, reclen;
		struct __kernel_dirent *de;

		count = lkl_sys_getdents(fd, (struct __kernel_dirent*) x, sizeof(x));
		assert(count > 0);

		de = (struct __kernel_dirent*) x;
		while (count > 0) {
			reclen = de->d_reclen;
			printf("%s %ld\n", de->d_name, de->d_ino);
			de = (struct __kernel_dirent*) ((char*) de+reclen);
			count-=reclen;
		}

		lkl_sys_close(fd);
	}
	printf("++++++++ done printing contents of [%s]\n", path);
}

void test_timer(void)
{
	struct __kernel_timespec ts = { .tv_sec = 1};
	int i;
	for(i = 3; i > 0; i--) {
		printf("Shutdown in %d \r", i); fflush(stdout);
		lkl_sys_nanosleep(&ts, NULL);
	}
}

void test_file_system(const char * mntstr)
{
	int fd, tmp;
	char buffer[5098];
	strcpy(buffer, mntstr);
	strcat(buffer, "CREDITS");
	// this test thinks that there's a "CREDITS" file in the root
	// of the mounted disk

	fd = lkl_sys_open(buffer, O_RDONLY, 0);
	while ((tmp = lkl_sys_read(fd, buffer, sizeof(buffer))) > 0)
		write(1, buffer, tmp);
	lkl_sys_close(fd);

	list_files("."); // "." should be equal to "/"
	list_files(mntstr);
}

int main(int argc, char **argv, char **env)
{
	char mnt_str[]= { "/dev/xxxxxxxxxxxxxxxx" };

#ifdef CONFIG_LKL_ENV_APR
	apr_init();
#endif
	lkl_env_init(16*1024*1024);

	mount_disk("disk", mnt_str, sizeof(mnt_str));
	test_file_system(mnt_str);
	umount_disk();

	mount_disk("disk", mnt_str, sizeof(mnt_str));
	test_file_system(mnt_str);
	umount_disk();

	test_timer();
	lkl_env_fini();

        return 0;
}
