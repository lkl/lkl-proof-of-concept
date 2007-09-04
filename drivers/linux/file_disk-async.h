#ifndef _FILE_DISK_ASYNC_H
#define _FILE_DISK_ASYNC_H

#define FILE_DISK_IRQ 1
#define FILE_DISK_MAJOR 42

struct completion_status {
	void *linux_cookie, *native_cookie;
	int status;
};

void* _file_open(void);
unsigned long _file_sectors(void);
void _file_rw_async(void *f, unsigned long sector, unsigned long nsect, 
		    char *buffer, int dir, struct completion_status *cs);

#endif
