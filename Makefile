a.out: a.c ../linux-2.6/vmlinux
	gcc -Wall $^ -lpthread
