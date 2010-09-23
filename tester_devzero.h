#ifndef _LKL_TESTER_DEV_ZERO_H
#define _LKL_TESTER_DEV_ZERO_H

#include <unistd.h>

#include <asm/lkl.h>
#include <linux/kdev_t.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


static char buff[1024*1024*32];

void test_dev_zero()
{
	int fd, i, j, num, err;
	int sizes[] = { 1, 1024, 10*1024, 100*1024, 1024*1024, 4*1024*1024, 8*1024*1024, 16*1024*1024, 32*1024*1024 };

	if (cla.lkl) {
		err = lkl_sys_mknod("/devzero", S_IFCHR|0600, MKDEV(1,5));
		if (err) {
			fprintf(stderr, "%s: failed to create /dev/zero: %s\n", __func__, strerror(-err));
			return;
		}
		fd = lkl_sys_open("/devzero", O_RDONLY, 0);
		if (fd < 0)
			errno = -fd;
	} else {
		fd = open("/dev/zero", O_RDONLY, 0);
	}

	if (fd < 0) {
		fprintf(stderr, "%s: failed to open /dev/zero: %s\n", __func__, strerror(errno));
		return;
	}

	for(i = 0; i< sizeof(sizes)/sizeof(int); i++) {
		printf("%s: reading %d bytes from /dev/zero...", __func__, sizes[i]);

		for(j = 0; j < 100; j++) {
			test_mark_start(0);
			if (cla.lkl)
				num = lkl_sys_read(fd, buff, sizes[i]);
			else
				num = read(fd, buff, sizes[i]);
			test_mark_stop(0);
			if (num != sizes[i]) {
				fprintf(stderr, "%s: short devzero read?\n", __func__);
				return;
			}
			take_sample(test_time(0));
		}

		printf("%f\n", samples_avg());
	}
}

#endif
