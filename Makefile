

CFLAGS=-g
HERE=$(PWD)
LINUX=$(HERE)/../linux-2.6

all: a.exe a.out a-aio.out a.sys

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

INC=include/asm include/asm-generic include/asm-i386 include/linux 

%.vmlinux %.env: %.config  
	mkdir -p $(patsubst %.config,%,$<) && \
	cp $< $(patsubst %.config,%,$<)/.config && \
	cd $(LINUX) && \
	$(MAKE) O=$(HERE)/$(patsubst %.config,%,$<) ARCH=lkl && \
	cd $(HERE) && \
	cp $(patsubst %.config,%,$<)/vmlinux $(patsubst %.config,%,$<).vmlinux &&	\
	cp $(patsubst %.config,%,$<)/env.a $(patsubst %.config,%,$<).env	

VFS = vfs.c posix.vmlinux posix.env 
NET = net.c linux.vmlinux linux.env

vfs: $(VFS) $(INC)
	gcc -Wall -g -Iinclude $(VFS) -lpthread -o $@

net: $(NET) $(INC)
	gcc -Wall -g -Iinclude $(NET) -lpthread -o $@

clean:
	-rm -rf lkl lkl-nt lkl-aio lkl-ntk
	-rm -f a.sys a-aio.out a.exe a.out apr.exe apr.out include/asm \
		include/asm-i386  include/asm-generic include/asm-linux 

.force:
