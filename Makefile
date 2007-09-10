CFLAGS=-g
HERE=$(PWD)
LINUX=$(HERE)/../linux-2.6

#all: a.exe apr.exe a.out apr.out
all: a.exe a.out apr.out a-aio.out

include/asm:
	-mkdir `dirname $@`
	ln -s $(LINUX)/include/asm-lkl include/asm

include/asm-i386:
	-mkdir `dirname $@`
	ln -s $(LINUX)/include/asm-i386 include/asm-i386

include/asm-generic:
	-mkdir `dirname $@`
	ln -s $(LINUX)/include/asm-generic include/asm-generic

include/linux:
	-mkdir `dirname $@`
	ln -s $(LINUX)/include/linux include/linux

%.config: $(LINUX)/arch/lkl/defconfig
	-mkdir `dirname $@`
	cp $^ $@

INC=include/asm include/asm-generic include/asm-i386 include/linux

lkl-aio/vmlinux: .force lkl-aio/.config
	cd $(LINUX) && \
	make O=$(HERE)/`dirname $@` ARCH=lkl \
		LKL_DRIVERS="$(HERE)/drivers/posix-aio/lkl/ $(HERE)/drivers/stduser/lkl" \
		EXTRA_CFLAGS=-DNR_IRQS=2 \
		STDIO_CONSOLE=y FILE_DISK_MAJOR=42 \
		vmlinux   	

lkl/vmlinux: .force lkl/.config
	cd $(LINUX) && \
	make O=$(HERE)/`dirname $@` ARCH=lkl \
		LKL_DRIVERS=$(HERE)/drivers/stduser/lkl \
		STDIO_CONSOLE=y FILE_DISK=y FILE_DISK_MAJOR=42 \
		vmlinux

lkl-nt/vmlinux: .force lkl-nt/.config
	cd $(LINUX) && \
	make O=$(HERE)/`dirname $@` ARCH=lkl CROSS_COMPILE=i586-mingw32msvc- \
		LKL_DRIVERS=$(HERE)/drivers/stduser/lkl \
		STDIO_CONSOLE=y FILE_DISK=y FILE_DISK_MAJOR=42 \
		vmlinux 

DRV_STDUSER=drivers/stduser/disk.c drivers/stduser/console.c 
DRV_AIO=drivers/stduser/console.c drivers/posix-aio/disk-async.c 

AOUT=stduser-main.c posix.c $(DRV_STDUSER) lkl/vmlinux
AOUT-aio=stduser-main.c $(DRV_AIO) posix.c lkl-aio/vmlinux
APROUT=stduser-main.c $(DRV_STDUSER) apr.c lkl/vmlinux
AEXE=stduser-main.c $(DRV_STDUSER) nt.c lkl-nt/vmlinux
APREXE=stduser-main.c $(DRV_STDUSER) apr.c lkl-nt/vmlinux

a-aio.out: $(AOUT-aio) $(INC)
	gcc -Wall -g -Iinclude $(AOUT-aio) -lpthread -lrt -DFILE_DISK_MAJOR=42 \
		-o $@

a.out: $(AOUT) $(INC)
	gcc -Wall -g -Iinclude $(AOUT) -lpthread -DFILE_DISK_MAJOR=42 -o $@

apr.out: $(APROUT) $(INC)
	gcc -Wall -g -Iinclude -I/usr/include/apr-1.0/ -D_LARGEFILE64_SOURCE \
		$(APROUT) -L/usr/lib/debug/usr/lib/libapr-1.so.0.2.7 -lapr-1 \
		-DFILE_DISK_MAJOR=42 -o $@   

a.exe: $(AEXE) $(INC)
	i586-mingw32msvc-gcc -g -Wall -Iinclude $(AEXE) -DFILE_DISK_MAJOR=42 \
		-o $@ 


apr.exe: $(APREXE) $(INC)
	i586-mingw32msvc-gcc -g -Wall -Iinclude -I/usr/include/apr-1.0/ \
		$(APREXE) -DFILE_DISK_MAJOR=42 -o $@

clean:
	-rm -rf a-aio.out a.exe a.out apr.exe apr.out lkl lkl-nt lkl-aio include

.force:
