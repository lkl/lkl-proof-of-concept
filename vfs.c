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
#endif	/* CONFIG_LKL_ENV_APR */


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

static char mode2kind(unsigned mode)
{
	switch(mode & S_IFMT){
	case S_IFSOCK: return 's';
	case S_IFLNK: return 'l';
	case S_IFREG: return '-';
	case S_IFDIR: return 'd';
	case S_IFBLK: return 'b';
	case S_IFCHR: return 'c';
	case S_IFIFO: return 'p';
	default: return '?';
	}
}

static void mode2str(unsigned mode, char *out)
{
	*out++ = mode2kind(mode);

	*out++ = (mode & 0400) ? 'r' : '-';
	*out++ = (mode & 0200) ? 'w' : '-';
	if(mode & 04000) {
		*out++ = (mode & 0100) ? 's' : 'S';
	} else {
		*out++ = (mode & 0100) ? 'x' : '-';
	}
	*out++ = (mode & 040) ? 'r' : '-';
	*out++ = (mode & 020) ? 'w' : '-';
	if(mode & 02000) {
		*out++ = (mode & 010) ? 's' : 'S';
	} else {
		*out++ = (mode & 010) ? 'x' : '-';
	}
	*out++ = (mode & 04) ? 'r' : '-';
	*out++ = (mode & 02) ? 'w' : '-';
	if(mode & 01000) {
		*out++ = (mode & 01) ? 't' : 'T';
	} else {
		*out++ = (mode & 01) ? 'x' : '-';
	}
	*out = 0;
}

int read_stat64(char * path)
{
	char mode[200];
	struct __kernel_stat64 stat;
	int rc = lkl_sys_stat64(path, &stat);
	if (rc != 0) {
		printf("sys_stat64(%s) error=%d strerr=[%s]\n", path, rc, strerror(-rc));
		return rc;
	}
	mode2str(stat.st_mode, mode);
	printf("%s %5lu %5d --- [%s]\n",
		   (char *) mode, (unsigned long) stat.st_size, (int)stat.st_uid, path);
	return 0;
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
			char fullpath[4096];
			fullpath[0] = '\0';
			strncat(fullpath, path, sizeof(fullpath));
			strncat(fullpath, "/",sizeof(fullpath));
			strncat(fullpath, de->d_name,sizeof(fullpath));
			read_stat64(fullpath);
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
