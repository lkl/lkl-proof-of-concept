#ifndef _LKL_TESTER_FS_H
#define _LKL_TESTER_FS_H

#include <unistd.h>
#include <sys/syscall.h>

#include <asm/lkl.h>
#include <asm/disk.h>
#include <asm/env.h>


static char fs_path[64];
static __kernel_dev_t fs_dev;
static FILE *fs_file;
static int fd;

static char* get_abs_path(const char *s)
{
	static char buff[1024];

	snprintf(buff, sizeof(buff), "%s/%s", fs_path, s);
	return buff;
}

#define min(a,b) ((a) > (b) ? b : a)

static inline void do_complete_read(int fd, int len)
{
	char buff[1024*1024];
	int i;

	while (len > 0) {
		if (cla.lkl)
			i = lkl_sys_read(fd, buff, min(len, sizeof(buff)));
		else
			i = read(fd, buff, min(len, sizeof(buff)));
		if (i <= 0) {
			fprintf(stderr, "%s: read failed: %s\n", __func__, strerror(cla.lkl?-i:errno));
			return;
		}
		len -= i;
	}
}

struct stat;
int lstat(const char *path, struct stat *buf);
int open(const char *, int, ...);
int readdir(unsigned int fd, struct __kernel_dirent *dirp,
	    unsigned int count);

static int finds = 0;

int recursive_find(const char *path)
{
	int err;
	int fd;

	if (cla.lkl)
		fd = lkl_sys_open(path, O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0);
	else
		fd = open(path, O_RDONLY|O_LARGEFILE|O_DIRECTORY, 0);

	if (fd < 0) {
		fprintf(stderr, "%s: failed to open %s: %s", __func__, path, strerror(cla.lkl?-fd:errno));
		return -1;
	}

	do {
		int count;
		struct __kernel_dirent *de;
		char x[300 * sizeof(*de)];


		if (cla.lkl)
			count = lkl_sys_getdents(fd, (struct __kernel_dirent*) x, sizeof(x));
		else
			count = syscall(141, fd, x, sizeof(x));

		if (count < 0) {
			fprintf(stderr, "%s: getdents failed: %s\n", __func__, strerror(cla.lkl?-count:errno));
			return -1;
		}

		for(de = (struct __kernel_dirent*) x; (char*)de < ((char*)x + count) && de->d_reclen;
		    de = (struct __kernel_dirent*)((char*)de + de->d_reclen)) {
			char tmp[1024] = { '@', };
			mode_t mode;

			if (strcmp(de->d_name, ".") == 0 ||
			    strcmp(de->d_name, "..") == 0)
				continue;

			snprintf(tmp, sizeof(tmp), "%s/%s", path, de->d_name);
			if (cla.lkl) {
				struct __kernel_stat stat;
				err = lkl_sys_newstat(tmp, &stat);
				mode = stat.st_mode;
			} else {
				struct __kernel_stat stat;
				err = syscall(106, tmp, &stat);
				mode = stat.st_mode;
			}

			if (err < 0) {
				fprintf(stderr, "%s: stat failed for %s: %s\n", __func__, tmp, strerror(cla.lkl?-err:errno));
				return -1;
			}

			if (mode & S_IFDIR) {
				err = recursive_find(tmp);
				if (err != 0)
					return err;
			} else {
				finds++;
			}
		}
	} while (0);

	if (cla.lkl)
		lkl_sys_close(fd);
	else
		close(fd);

	return 0;
}

static void test_fs_find(void)
{
	int i;

	//warm-up
	recursive_find(get_abs_path(""));

	printf("finding files..."); fflush(stdout);
	for(i = 0; i < 100; i++) {
		test_mark_start(0);
		recursive_find(get_abs_path(""));
		test_mark_stop(0);
		take_sample(test_time(0));
	}
	printf(" found %d files in  %f\n", finds/101, samples_avg());
}

void test_fs_read(void)
{
	int i, j, sizes[] = { 16*1024*1024, 32*1024*1024, 64*1024*1024, 128*1024*1024 };

	for(i = 0; i < sizeof(sizes)/sizeof(int); i++) {
		printf("reading %d from %s... ", sizes[i], get_abs_path("a")); fflush(stdout);
		for(j = 0; j < 100; j++) {
			test_mark_start(0);
			if (cla.lkl)
				fd = lkl_sys_open(get_abs_path("a"), O_RDONLY, 0);
			else
				fd = open(get_abs_path("a"), O_RDONLY, 0);
			if (fd < 0) {
				fprintf(stderr, "%s: open failed: %s\n", __func__, get_abs_path("a"));
				return;
			}
			do_complete_read(fd, sizes[i]);
			if (cla.lkl)
				lkl_sys_close(fd);
			else
				close(fd);
			test_mark_stop(0);
			take_sample(test_time(0));
		}
		printf("%f\n", samples_avg());
	}
}

void test_fs_seek(void)
{
        static const int seeks = 1024;
	int i, j;

	srandom(0x1234);
	printf("doing %d seeks... ", seeks); fflush(stdout);
	for(j = 0; j < 100; j++) {
		test_mark_start(0);
		if (cla.lkl)
			fd = lkl_sys_open(get_abs_path("a"), O_RDONLY, 0);
		else
			fd = open(get_abs_path("a"), O_RDONLY, 0);
		if (fd < 0) {
			fprintf(stderr, "%s: open failed: %s\n", __func__, get_abs_path("a"));
			return;
		}
		for(i = 0; i < seeks; i++) {
			if (cla.lkl)
				lkl_sys_lseek(fd, random() % 127*1024*1024, SEEK_SET);
			else
				lseek(fd, random() % 127*1024*1024, SEEK_SET);
			do_complete_read(fd, 512);
		}
		if (cla.lkl)
			lkl_sys_close(fd);
		else
			close(fd);
		test_mark_stop(0);
		take_sample(test_time(0));
	}
	printf("%f\n", samples_avg());
}


int fs_pre_test(void)
{
	int rc = 0;

	if (!cla.fsimg)
		return rc;

	if (cla.lkl) {
		fs_file = fopen(cla.fsimg, "r+b");
		if (!fs_file) {
			fprintf(stderr, "%s: lkl: failed to open image %s: %s\n",
				__func__, cla.fsimg, strerror(errno));
			return -1;
		}
		fs_dev = lkl_disk_add_disk(fs_file, 20480000);
		if (!fs_dev) {
			fprintf(stderr, "%s: lkl: failed to add disk\n",
				__func__);
			return -1;
		}

		rc = lkl_mount_dev(fs_dev, NULL, 0, NULL, fs_path, sizeof(fs_path));
		if (rc) {
			fprintf(stderr, "%s: lkl: failed to mount disk: %s\n",
				__func__, strerror(-rc));
			return rc;
		}

	} else if (cla.loop) {
		char tmp[1024], *tmp_dir;

		tmp_dir = mkdtemp("/tmp/lkl_tester_XXXXXX");
		if (!tmp_dir) {
			fprintf(stderr, "%s: loop: failed to create temp dir: %s\n",
				__func__, strerror(errno));
			return -1;
		}
		strcpy(fs_path, tmp_dir);

		snprintf(tmp, sizeof(tmp), "mount %s %s -o loop", cla.fsimg, fs_path);
		rc = system(tmp);
		if (rc < 0)
			fprintf(stderr, "%s: loop: failed to mount loop\n", __func__);
	} else if (cla.native) {
		char tmp[1024], *tmp_dir;

		tmp_dir = mkdtemp("/tmp/lkl_tester_XXXXXX");
		if (!tmp_dir) {
			fprintf(stderr, "%s: native: failed to create temp dir: %s\n",
				__func__, strerror(errno));
			return -1;
		}
		strcpy(fs_path, tmp_dir);

		snprintf(tmp, sizeof(tmp), "mount %s /mnt/tmp -o loop; cp -rp /mnt/tmp/* %s; umount /mnt/tmp", cla.fsimg, fs_path);
		rc = system(tmp);
		if (rc < 0)
			fprintf(stderr, "%s: loop: failed to mount loop\n", __func__);
	} else
		rc = -1;

	return rc;
}

void fs_post_test(void)
{
	if (!cla.fsimg)
		return;

	if (cla.lkl) {
		lkl_umount_dev(fs_dev, 0);
		lkl_disk_del_disk(fs_dev);
		fclose(fs_file);
	} else if (cla.loop) {
		char tmp[1024];

		snprintf(tmp, sizeof(tmp), "umount %s", fs_path);
		system(tmp);
		rmdir(fs_path);
	} else if (cla.native) {
		char tmp[1024];

		snprintf(tmp, sizeof(tmp), "rm -rf %s", fs_path);
		system(tmp);
	}
}

#endif
