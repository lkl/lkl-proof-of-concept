#include <stdlib.h>
#include <error.h>
#include <argp.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>

static char doc[] = "LKL tester";

/* A description of the arguments we accept. */
static char args_doc[] = "";

/* The options we understand. */
static struct argp_option options[] = {
	{"lkl", 'l', 0, 0, "use lkl APIs"},
        {"native", 'n', 0, 0, "use native APIs"},
        {"loop", 'o', 0, 0, "use native APIs and loopback device"},
	{"fsimg", 'f', "file", 0, "disk image file"},
	{"test", 't', "test", 0, "test name"},
	{"interface", 'i', "interface", 0, "interface to use for network tests"},
	{0}
};

struct cl_args {
	int lkl:1, native:1, loop:1;
	const char *fsimg;
	void (*test)(void);
};

static struct cl_args cla = {
	.lkl = 0,
	.native = 1,
	.loop = 0,
	.fsimg  = NULL,
};

struct test {
	const char *name;
	void (*f)(void);
	int fs_test;
};


struct timeval tv[100][2];

static inline void test_mark_start(int i)
{
	gettimeofday(&tv[i][0], NULL);
}

static inline void test_mark_stop(int i)
{
	gettimeofday(&tv[i][1], NULL);
}

static inline unsigned long long test_time(int i)
{
	return (tv[i][1].tv_sec - tv[i][0].tv_sec) * 1000000000ULL +
		tv[i][1].tv_usec - tv[i][0].tv_usec;
}

static struct sample {
	int num;
	long long values[1000];
        float avg;
} samples;

static inline void take_sample(unsigned long long value)
{
	samples.values[samples.num++] = value;
}

static inline float samples_avg(void)
{
	int i, j = 0;
	double avg = 0, ratio;

	avg = samples.values[0];
	for(i = 1; i < samples.num; i++) {
		if (samples.values[i] > samples.values[i-1])
			ratio = (double)samples.values[i] / samples.values[i-1];
		else
			ratio = (double)samples.values[i-1] / samples.values[i];
		if (ratio < 2) {
			avg += samples.values[i];
			j++;
		}
	}

	avg /= j;

	samples.num = 0;

	return avg;

}

#include "tester_fs.h"
#include "tester_basic.h"
#include "tester_devzero.h"

static struct test tests[] = {
	{ "syscall_latency", test_syscall_latency, 0},
	{ "devzero", test_dev_zero, 0},
	{ "read", test_fs_read, 1},
	{ "seek", test_fs_seek, 1},
	{ "find", test_fs_find, 1},
};

/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct cl_args *cla = state->input;

	switch (key) {
	case 'l':
		cla->lkl = 1;
		break;
	case 'n':
		cla->native = 1;
		break;
	case 'o':
		cla->loop = 1;
		cla->native = 1;
		break;
	case 'f':
		cla->fsimg = arg;
		break;
	case 't':
	{
		int i;

		for(i = 0; i < sizeof(tests)/sizeof(struct test); i++) {
			if (strcmp(tests[i].name, arg) == 0) {
				if (tests[i].fs_test) {
					static char fsimg[1024];
					snprintf(fsimg, sizeof(fsimg), "%s.img", tests[i].name);
					if (!cla->fsimg)
						cla->fsimg = fsimg;
				}
				cla->test = tests[i].f;
				return 0;
			}
		}

		fprintf(stderr, "invalid test %s, valid tests are: \n", arg);
		for(i = 0; i < sizeof(tests)/sizeof(struct test); i++) {
			fprintf(stderr, "%s\n", tests[i].name);
		}

		return -1;
	}
	case ARGP_KEY_ARG:
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv)
{
	int err;

	err= argp_parse(&argp, argc, argv, 0, 0, &cla);
	if (err < 0)
		return err;

	if (cla.lkl) {
		err = lkl_env_init(16*1024*1024);
		if (err < 0)
			return 1;
	}

	err = fs_pre_test();
	if (err < 0)
		return 1;

	if (!cla.test) {
		fprintf(stderr, "no test to run\n");
		return 1;
	}

	cla.test();

	fs_post_test();

	if (cla.lkl)
		lkl_env_fini();

	return 0;
}

