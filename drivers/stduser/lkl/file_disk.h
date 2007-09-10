#ifndef _FILE_DISK_H
#define _FILE_DISK_H

void* _file_open(void);
unsigned long _file_sectors(void);
void _file_rw(void *f, unsigned long sector, unsigned long nsect, char *buffer, int dir);

#endif
