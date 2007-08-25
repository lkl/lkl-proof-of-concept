
all: a.exe a.out

linux-native/.config: ../linux-2.6/arch/lkl/defconfig
	-mkdir `dirname $@`
	cp $^ $@

linux-mingw/.config: ../linux-2.6/arch/lkl/defconfig
	-mkdir `dirname $@`
	cp $^ $@

linux-native/vmlinux: .force linux-native/.config
	cd ../linux-2.6 && make O=../poc/linux-native ARCH=lkl vmlinux
a.out: a.c linux-native/vmlinux
	gcc -Wall -g  $^ -lpthread

linux-mingw/vmlinux: .force linux-mingw/.config
	cd ../linux-2.6 && make O=../poc/linux-mingw ARCH=lkl CROSS_COMPILE=i586-mingw32msvc- vmlinux
a.exe: b.c linux-mingw/vmlinux
	i586-mingw32msvc-gcc -g -Wall $^ 

clean:
	rm -rf a.exe a.out linux-native linux-mingw

.force: 
