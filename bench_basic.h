#ifndef _LKL_TESTER_GETPID_H
#define _LKL_TESTER_GETPID_H

#include <unistd.h>

#include <asm/lkl.h>


static void run_syscall(const char *name, long (*f)(void))
{
	int i;

	printf("%s latency ...", name); fflush(stdout);

	for(i = 0; i < 10000; i++) {
		test_mark_start(0);
		f();
		test_mark_stop(0);
		take_sample(test_time(0));
	}

	printf("%f\n", samples_avg());
}

#define RUN_SYSCALL_LATENCY(syscall)					\
	if (cla.lkl)							\
		run_syscall("lkl_"#syscall, (long(*)(void))lkl_sys_##syscall); \
	else								\
		run_syscall(#syscall, (long(*)(void))syscall);


void test_syscall_latency()
{
	RUN_SYSCALL_LATENCY(getpid);
	RUN_SYSCALL_LATENCY(getuid);
	RUN_SYSCALL_LATENCY(getgid);
}

#endif
