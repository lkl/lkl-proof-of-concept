CFLAGS=-g
HERE=$(PWD)
LINUX=$(HERE)/../linux-2.6


#all: a.exe apr.exe a.out apr.out
all: a.exe a.out apr.out a-async.out
#all:a-async.out

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

linux-async/vmlinux: .force linux-async/.config
	cd $(LINUX) && $(MAKE) O=$(HERE)/linux-async ARCH=lkl LKL_DRIVERS=$(HERE)/drivers/linux ASYNC=-async EXTRA_CFLAGS=-DNR_IRQS=2 vmlinux

linux/vmlinux: .force linux/.config
	cd $(LINUX) && $(MAKE) O=$(HERE)/linux ARCH=lkl LKL_DRIVERS=$(HERE)/drivers/linux vmlinux

linux-mingw/vmlinux: .force linux-mingw/.config
	cd $(LINUX) && $(MAKE) O=$(HERE)/linux-mingw ARCH=lkl CROSS_COMPILE=i586-mingw32msvc- LKL_DRIVERS=$(HERE)/drivers/linux vmlinux

COMMON_SOURCES=main.c drivers/disk.c drivers/console.c
AOUT=$(COMMON_SOURCES) threads-posix.c linux/vmlinux
AOUT-async=$(patsubst %disk.c,%disk-async.c,  $(COMMON_SOURCES)) threads-posix.c linux-async/vmlinux
APROUT=$(COMMON_SOURCES) threads-apr.c linux/vmlinux
AEXE=$(COMMON_SOURCES) threads-windows.c linux-mingw/vmlinux
APREXE=$(COMMON_SOURCES) threads-apr.c linux-mingw/vmlinux

a-async.out: $(AOUT-async) $(INC)
	gcc -Wall -g -Iinclude $(AOUT-async) -lpthread -lrt -o $@

a.out: $(AOUT) $(INC)
	gcc -Wall -g -Iinclude $(AOUT) -lpthread -o $@

apr.out: $(APROUT) $(INC)
	gcc -Wall -g -Iinclude -I/usr/include/apr-1.0/ -D_LARGEFILE64_SOURCE $(APROUT) -L/usr/lib/debug/usr/lib/libapr-1.so.0.2.7 -lapr-1 -o $@

a.exe: $(AEXE) $(INC)
	i586-mingw32msvc-gcc -g -Wall -Iinclude $(AEXE)

apr.exe: $(APREXE) $(INC)
	i586-mingw32msvc-gcc -g -Wall -Iinclude -I/usr/include/apr-1.0/ $(APREXE)

clean:
	-rm -rf a.exe a.out apr.exe apr.out linux linux-mingw linux-async include

.force:
