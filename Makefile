CFLAGS=-g
HERE=$(PWD)
LINUX=$(HERE)/../linux-2.6

#all: a.exe apr.exe a.out apr.out
all: a.exe a.out apr.out


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

linux-native/.config: $(LINUX)/arch/lkl/defconfig
	-mkdir `dirname $@`
	cp $^ $@

linux-mingw/.config: $(LINUX)/arch/lkl/defconfig
	-mkdir `dirname $@`
	cp $^ $@

INC=include/asm include/asm-generic include/asm-i386 include/linux

linux-native/vmlinux: .force linux-native/.config
	cd $(LINUX) && make O=$(HERE)/linux-native ARCH=lkl LKL_DRIVERS=$(HERE)/drivers/linux vmlinux
COMMON_SOURCES=main.c drivers/disk.c drivers/console.c
AOUT=$(COMMON_SOURCES) threads-posix.c linux-native/vmlinux
APROUT=$(COMMON_SOURCES) threads-apr.c linux-native/vmlinux
a.out: $(AOUT) $(INC)
	gcc -Wall -g -Iinclude $(AOUT) -lpthread -o $@
apr.out: $(APROUT) $(INC)
	gcc -Wall -g -Iinclude -I/usr/include/apr-1.0/ -D_LARGEFILE64_SOURCE $(APROUT) -L/usr/lib/debug/usr/lib/libapr-1.so.0.2.7 -lapr-1 -o $@
linux-mingw/vmlinux: .force linux-mingw/.config
	cd $(LINUX) && make O=$(HERE)/linux-mingw ARCH=lkl CROSS_COMPILE=i586-mingw32msvc- LKL_DRIVERS=$(HERE)/drivers/linux vmlinux
AEXE=$(COMMON_SOURCES) threads-windows.c linux-mingw/vmlinux
APREXE=$(COMMON_SOURCES) threads-apr.c linux-mingw/vmlinux

a.exe: $(AEXE) $(INC)
	i586-mingw32msvc-gcc -g -Wall -Iinclude $(AEXE)
apr.exe: $(APREXE) $(INC)
	i586-mingw32msvc-gcc -g -Wall -Iinclude -I/usr/include/apr-1.0/ $(APREXE)
clean:
	rm -rf a.exe a.out apr.exe apr.out linux-native linux-mingw include

.force:
