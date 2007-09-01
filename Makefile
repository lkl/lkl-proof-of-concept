
HERE=$(PWD)
LINUX=$(HERE)/../linux-2.6

all: a.exe a.out

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
AOUT=main.c threads-posix.c drivers/disk.c drivers/console.c linux-native/vmlinux 
a.out: $(AOUT) $(INC) 
	gcc -Wall -g -Iinclude $(AOUT) -lpthread

linux-mingw/vmlinux: .force linux-mingw/.config
	cd $(LINUX) && make O=$(HERE)/linux-mingw ARCH=lkl CROSS_COMPILE=i586-mingw32msvc- LKL_DRIVERS=$(HERE)/drivers/linux vmlinux
AEXE=main.c threads-windows.c drivers/disk.c drivers/console.c linux-mingw/vmlinux 
a.exe: $(AEXE) $(INC)
	i586-mingw32msvc-gcc -g -Wall -Iinclude $(AEXE)

clean:
	rm -rf a.exe a.out linux-native linux-mingw include

.force: 
